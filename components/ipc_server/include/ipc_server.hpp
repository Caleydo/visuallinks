/*!
 * @file ipc_server.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#ifndef _IPC_SERVER_HPP_
#define _IPC_SERVER_HPP_

#include <QtCore>
#include <QtOpenGL/qgl.h>
#include <qwindowdefs.h>

#include "common/componentarguments.h"
#include "config.h"
#include "linkdescription.h"
#include "slotdata/component_selection.hpp"
#include "slotdata/image.hpp"
#include "slotdata/mouse_event.hpp"
#include "slotdata/polygon.hpp"
#include "slotdata/text_popup.hpp"
#include "window_monitor.hpp"

#include "QWsServer.h"
#include "QWsSocket.h"

#include "datatypes.h"
#include <stdint.h>

class QMutex;
class JSONParser;

namespace LinksRouting
{
  class ClientInfo;

  class IPCServer:
    public QObject,
    public Component,
    public ComponentArguments

  {
      Q_OBJECT

    public:

      IPCServer(QMutex* mutex, QWaitCondition* cond_data, QWidget* widget);
      virtual ~IPCServer();

      void publishSlots(SlotCollector& slot_collector);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool startup(Core* core, unsigned int type);
      void init();
      void shutdown();
      bool supports(unsigned int type) const
      {
        return (type & Component::DataServer);
      }

      uint32_t process(unsigned int type) override;

      int getPreviewWidth() const { return _preview_width; }
      int getPreviewHeight() const { return _preview_height; }
      bool getPreviewAutoWidth() const { return _preview_auto_width; }
      bool getOutsideSeeThrough() const { return _outside_see_through; }
      QRect& desktopRect() const { return *_subscribe_desktop_rect->_data; }

      typedef SlotType::TextPopup::Popups::iterator PopupIterator;

      PopupIterator addPopup( const ClientInfo& client_info,
                              const SlotType::TextPopup::Popup& popup );
      void removePopup(const PopupIterator& popup);
      void removePopups(const std::list<PopupIterator>& popups);

      typedef SlotType::XRayPopup::Popups::iterator XRayIterator;

      XRayIterator addCoveredPreview( const std::string& link_id,
                                      const ClientInfo& client_info,
                                      const LinkDescription::NodePtr& node,
                                      const HierarchicTileMapWeakPtr& tile_map,
                                      const QRect& viewport,
                                      const QRect& scroll_region,
                                      bool extend = false );
      void removeCoveredPreview(const XRayIterator& preview);
      void removeCoveredPreviews(const std::list<XRayIterator>& previews);

      typedef SlotType::CoveredOutline::List::iterator OutlineIterator;

      OutlineIterator addOutline(const ClientInfo& client_info);
      void removeOutline(const OutlineIterator& outline);
      void removeOutlines(const std::list<OutlineIterator>& outlines);

    private slots:

      void onClientConnection();
      void onTextReceived(QString data);
      void onBinaryReceived(QByteArray data);
      void onPong(quint64 elapsedTime);
      void onClientDisconnection();

    protected:

      typedef std::map<QWsSocket*, std::unique_ptr<ClientInfo>> ClientInfos;

      void onInitiate( LinkDescription::LinkList::iterator& link,
                       const QString& id,
                       uint32_t stamp,
                       const JSONParser& msg,
                       ClientInfo& client_info );

      void regionsChanged(const WindowRegions& regions);
      ClientInfos::iterator findClientInfo(WId wid);
      QWsSocket* getSocketByWId(WId wid);

      bool updateHedge( const WindowRegions& regions,
                        LinkDescription::HyperEdge* hedge );
      bool updateCenter(LinkDescription::HyperEdge* hedge);
      bool updateRegion( const WindowRegions& regions,
                         LinkDescription::Node* node,
                         WId client_wid );

      void onClick(int x, int y);
      void onMouseMove(int x, int y);
      void onScrollWheel(int delta, const float2& pos, uint32_t mod);
      void onDrag(const float2& delta);

      typedef std::function<bool( SlotType::TextPopup::Popup&,
                                  QWsSocket&,
                                  ClientInfo& )> popup_callback_t;
      bool foreachPopup(const popup_callback_t& cb);

      typedef std::function<bool( SlotType::XRayPopup::HoverRect&,
                                  QWsSocket&,
                                  ClientInfo& )> preview_callback_t;
      bool foreachPreview(const preview_callback_t& cb);

      void dirtyLinks();
      void dirtyRender();

    private:

      QWsServer          *_server;
      ClientInfos         _clients;
      WindowMonitor       _window_monitor;

      QMutex             *_mutex_slot_links;
      QWaitCondition     *_cond_data_ready;
      uint32_t            _dirty_flags;

      /* List of all open searches */
      slot_t<LinkDescription::LinkList>::type _slot_links;

      /* Outside x-ray popup */
      slot_t<SlotType::XRayPopup>::type _slot_xray;
      slot_t<SlotType::CoveredOutline>::type _slot_outlines;

      /* List of available routing components */
      slot_t<SlotType::ComponentSelection>::type _subscribe_routing;

      /* Permanent configuration changeable at runtime */
      slot_t<LinksRouting::Config*>::type _subscribe_user_config;

      /* Drawable desktop region */
      slot_t<QRect>::type _subscribe_desktop_rect;

      /* Slot for registering mouse callback */
      slot_t<SlotType::MouseEvent>::type _subscribe_mouse;
      slot_t<SlotType::TextPopup>::type _subscribe_popups;

      void updateScrollRegion( const JSONParser& json,
                               ClientInfo& client_info );

      /**
       * Clear all data for the given link
       */
      LinkDescription::LinkList::iterator abortLinking(
        const LinkDescription::LinkList::iterator& link =
              LinkDescription::LinkList::iterator()
      );

      /**
       * Clear all links
       */
      void abortAll();

      std::string   _debug_regions,
                    _debug_full_preview_path;
      QImage        _full_preview_img;
      int           _preview_width,
                    _preview_height;
      bool          _preview_auto_width,
                    _outside_see_through;

      struct InteractionHandler
      {
        IPCServer* _server;

        struct TileRequest
        {
          HierarchicTileMapWeakPtr tile_map;
          int zoom;
          size_t x, y;
          clock::time_point time_stamp;
        };

        typedef std::map<uint8_t, TileRequest> TileRequests;
        TileRequests  _tile_requests;
        uint8_t       _tile_request_id;

        InteractionHandler(IPCServer* server);
        bool updateRegion( QWsSocket* socket,
                           SlotType::TextPopup::Popup& popup,
                           float2 center = float2(-9999, -9999),
                           float2 rel_pos = float2() );
        void updateRegion( SlotType::XRayPopup::HoverRect& popup );

        protected:
          bool updateTileMap( const HierarchicTileMapPtr& tile_map,
                              QWsSocket* socket,
                              const Rect& rect,
                              int zoom );
      } _interaction_handler;
  };

} // namespace LinksRouting

#endif /* _IPC_SERVER_HPP_ */
