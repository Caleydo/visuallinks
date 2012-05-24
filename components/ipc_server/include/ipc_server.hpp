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
#include "linkdescription.h"
#include "slotdata/polygon.hpp"

#include "QWsServer.h"
#include "QWsSocket.h"

#include <QtCore>

#include "datatypes.h"

class QMutex;

namespace LinksRouting
{
  class IPCServer:
    public QObject,
    public ComponentArguments

  {
      Q_OBJECT

    public:

      IPCServer(QMutex* mutex);
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

      QMutex             *_mutex_slot_links;

      /* List of all open searches */
      slot_t<LinkDescription::LinkList>::type _slot_links;

      class JSON;
      std::vector<LinkDescription::Node> parseRegions(JSON& json);

      std::string   _debug_regions;

  };

} // namespace LinksRouting

#endif /* _IPC_SERVER_HPP_ */
