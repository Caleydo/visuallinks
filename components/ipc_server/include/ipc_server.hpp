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
#include <qwindowdefs.h>

#include "common/componentarguments.h"
#include "config.h"
#include "HierarchicTileMap.hpp"
#include "linkdescription.h"
#include "PartitionHelper.hxx"
#include "slotdata/component_selection.hpp"
#include "slotdata/image.hpp"
#include "slotdata/mouse_event.hpp"
#include "slotdata/polygon.hpp"
#include "slotdata/text_popup.hpp"
#include "window_monitor.hpp"

#include "QWsServer.h"
#include "QWsSocket.h"

#include "datatypes.h"

class QMutex;
class JSONParser;

namespace LinksRouting
{
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

    private slots:

      void onClientConnection();
      void onTextReceived(QString data);
      void onBinaryReceived(QByteArray data);
      void onPong(quint64 elapsedTime);
      void onClientDisconnection();

    protected:

      struct ClientInfo
      {
        WId     wid;
        QRect   region;
        QRect   scroll_region;
        Partitions           partitions;
        HierarchicTileMapPtr tile_map;
      };
      typedef std::map<QWsSocket*, ClientInfo> ClientInfos;

      void onInitiate( LinkDescription::LinkList::iterator& link,
                       const QString& id,
                       uint32_t stamp,
                       const JSONParser& msg,
                       ClientInfo& client_info );

      void regionsChanged(const WindowRegions& regions);
      bool updateHedge( const WindowRegions& regions,
                        LinkDescription::HyperEdge* hedge );
      bool updateCenter(LinkDescription::HyperEdge* hedge);
      bool updateRegion( const WindowRegions& regions,
                         LinkDescription::Node* node,
                         WId client_wid );

    private:

      QWsServer          *_server;
      ClientInfos         _clients;
      WindowMonitor       _window_monitor;

      QMutex             *_mutex_slot_links;
      QWaitCondition     *_cond_data_ready;

      /* List of all open searches */
      slot_t<LinkDescription::LinkList>::type _slot_links;

      /* List of available routing components */
      slot_t<SlotType::ComponentSelection>::type _subscribe_routing;

      /* Permanenet configuration changeable at runtime */
      slot_t<LinksRouting::Config*>::type _subscribe_user_config;

      /* Slot for registering mouse callback */
      slot_t<SlotType::MouseEvent>::type _subscribe_mouse;
      slot_t<LinksRouting::SlotType::TextPopup>::type _subscribe_popups;

      LinkDescription::NodePtr parseRegions( const JSONParser& json,
                                             ClientInfo& client_info );
      void updateScrollRegion( const JSONParser& json,
                               ClientInfo& client_info );

      std::string   _debug_regions;
      int           _preview_width,
                    _preview_height;

      struct InteractionHandler
      {
        IPCServer* _server;

        struct TileRequest
        {
          HierarchicTileMapWeakPtr tile_map;
          int zoom;
          size_t x, y;
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
