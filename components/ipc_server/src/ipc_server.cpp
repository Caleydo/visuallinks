/*!
 * @file ipc_server.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#include "ipc_server.hpp"
#include "log.hpp"

#include <QScriptEngine>
#include <QScriptValueIterator>

#include <map>
#include <tuple>

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  IPCServer::IPCServer()
  {

  }

  //----------------------------------------------------------------------------
  IPCServer::~IPCServer()
  {

  }

  //----------------------------------------------------------------------------
  bool IPCServer::startup(Core* core, unsigned int type)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  void IPCServer::init()
  {
    int port = 4487;
    _server = new QWsServer(this);
    if( !_server->listen(QHostAddress::Any, port) )
    {
      LOG_ERROR("Failed to start WebSocket server on port " << port << ": "
                << _server->errorString().toStdString() );
    }
    else
      LOG_INFO("WebSocket server listening on port " << port);

    connect(_server, SIGNAL(newConnection()), this, SLOT(onClientConnection()));
  }

  //----------------------------------------------------------------------------
  void IPCServer::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  void IPCServer::process(Type type)
  {

  }

  //----------------------------------------------------------------------------
  void IPCServer::onClientConnection()
  {
    QWsSocket* client_socket = _server->nextPendingConnection();
    QObject* client_object = qobject_cast<QObject*>(client_socket);

    connect(client_object, SIGNAL(frameReceived(QString)), this, SLOT(onDataReceived(QString)));
    connect(client_object, SIGNAL(disconnected()), this, SLOT(onClientDisconnection()));
    connect(client_object, SIGNAL(pong(quint64)), this, SLOT(onPong(quint64)));

    _clients << client_socket;

    LOG_INFO(   "Client connected: "
             << client_socket->peerAddress().toString().toStdString()
             << ":" << client_socket->peerPort() );
  }

  //----------------------------------------------------------------------------
  void IPCServer::onDataReceived(QString data)
  {
    std::cout << "Received: " << data.toStdString() << std::endl;
    std::map<std::string, std::tuple<std::string>> types
    {
      {"x", std::make_tuple("int")},
      {"y", std::make_tuple("int")},
      {"key", std::make_tuple("string")}
    };

    QScriptEngine engine;
    QScriptValue json = engine.evaluate("("+data+")");
    if( !json.isValid() )
    {
      LOG_WARN("Failed to evaluted received data.");
      return;
    }

    if( engine.hasUncaughtException() )
    {
      LOG_WARN("Failed to evaluted received data: "
               << json.toString().toStdString());
      return;
    }

    if( json.isArray() || !json.isObject() )
    {
      LOG_WARN("Received data is no JSON object.");
      return;
    }

    QScriptValueIterator it(json);
    while( it.hasNext() )
    {
      it.next();
      const std::string key = it.name().toStdString();
      const auto val_config = types.find(key);

      if( val_config == types.end() )
      {
        LOG_WARN("Unknown parameter (" << key << ")");
        continue;
      }

      const std::string type = std::get<0>(val_config->second);
      if(    ((type == "int" || type == "float") && !it.value().isNumber())
          || (type == "string" && !it.value().isString()) )
      {
        LOG_WARN("Wrong type for parameter `" << key << "` ("
                 << type << " expected)");
        continue;
      }

      std::cout << "valid: "
                << key
                << "="
                << it.value().toString().toStdString()
                << std::endl;
    }
  }

  //----------------------------------------------------------------------------
  void IPCServer::onPong(quint64 elapsedTime)
  {

  }

  //----------------------------------------------------------------------------
  void IPCServer::onClientDisconnection()
  {
    QWsSocket * socket = qobject_cast<QWsSocket*>(sender());
    if( !socket )
      return;

    _clients.removeOne(socket);
    socket->deleteLater();

    LOG_INFO("Client disconnected");
  }

} // namespace LinksRouting
