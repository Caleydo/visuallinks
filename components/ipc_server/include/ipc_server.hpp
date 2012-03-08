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

#include "QWsServer.h"
#include "QWsSocket.h"

#include <QtCore>

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

//      void publishSlots(SlotCollector& slots);
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
  };

} // namespace LinksRouting

#endif /* _IPC_SERVER_HPP_ */
