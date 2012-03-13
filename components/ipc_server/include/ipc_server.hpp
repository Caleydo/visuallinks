/*!
 * @file ipc_server.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#ifndef _IPC_SERVER_HPP_
#define _IPC_SERVER_HPP_

#include "common/componentarguments.h"
#include "slotdata/polygon.hpp"

#include "QWsServer.h"
#include "QWsSocket.h"

#include <QtCore>

#include "datatypes.h"

namespace LinksRouting
{
  class IPCServer:
    public QObject,
    public ComponentArguments

  {
      Q_OBJECT

    public:

      IPCServer();
      virtual ~IPCServer();

      void publishSlots(SlotCollector& slot_collector);
//      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool startup(Core* core, unsigned int type);
      void init();
      void shutdown();
      bool supports(Type type) const
      {
        return type == Component::DataServer;
      }
      const std::string& name() const
      {
        static const std::string name("QtWebsocketServer");
        return name;
      }

      void process(Type type);

    private slots:

      void onClientConnection();
      void onDataReceived(QString data);
      void onPong(quint64 elapsedTime);
      void onClientDisconnection();

    private:

      QWsServer          *_server;
      QList<QWsSocket*>   _clients;

      /* Identifier for the current search */
      slot_t<std::string>::type _slot_search_id;

      /* Timestamp of current search */
      slot_t<uint32_t>::type    _slot_search_stamp;

      /* Regions of found occurances of search string */
      slot_t<std::vector<SlotType::Polygon>>::type  _slot_search_regions;

      class JSON;
      void parseRegions(JSON& json);

  };

} // namespace LinksRouting

#endif /* _IPC_SERVER_HPP_ */
