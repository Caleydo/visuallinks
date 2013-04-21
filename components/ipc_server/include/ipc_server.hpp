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

#include <chrono>

class QMutex;
class JSONParser;

namespace LinksRouting
{
  typedef std::chrono::high_resolution_clock clock;
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

      void process(unsigned int type);

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

    private slots:

      void onClientConnection();
      void onTextReceived(QString data);
      void onBinaryReceived(QByteArray data);
      void onPong(quint64 elapsedTime);
      void onClientDisconnection();

      void onPinnedAppsChanged();

    protected:

      typedef std::map<QWsSocket*, std::unique_ptr<ClientInfo>> ClientInfos;

      void onInitiate( LinkDescription::LinkList::iterator& link,
                       const QString& id,
                       uint32_t stamp,
                       const JSONParser& msg,
                       ClientInfo& client_info );

      void regionsChanged(const WindowRegions& regions);
      ClientInfos::iterator findClientInfo(WId wid);
      bool updateHedge( const WindowRegions& regions,
                        LinkDescription::HyperEdge* hedge );
      bool updateCenter(LinkDescription::HyperEdge* hedge);
      bool updateRegion( const WindowRegions& regions,
                         LinkDescription::Node* node,
                         WId client_wid );

      void addCoveredPreview( const LinkDescription::NodePtr& node,
                              const QRect& region,
                              const QRect& scroll_region,
                              LinkDescription::nodes_t& covered_nodes,
                              bool extend = false );

      void onClick(int x, int y);
      void onMouseMove(int x, int y);
      void onScrollWheel(int delta, const float2& pos, uint32_t mod);
      void onDrag(const float2& delta);

    private:

      QWsServer          *_server;
      ClientInfos         _clients;
      WindowMonitor       _window_monitor;

      QMutex             *_mutex_slot_links;
      QWaitCondition     *_cond_data_ready;

      /* List of all open searches */
      slot_t<LinkDescription::LinkList>::type _slot_links;

      /* Outside x-ray popup */
      slot_t<SlotType::XRayPopup>::type _slot_xray;

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
        void updateRegion( const ClientInfos::value_type& client_info,
                           SlotType::TextPopup::Popup& popup,
                           float2 center = float2(-9999, -9999),
                           float2 rel_pos = float2() );
      } _interaction_handler;
  };

} // namespace LinksRouting

#endif /* _IPC_SERVER_HPP_ */
