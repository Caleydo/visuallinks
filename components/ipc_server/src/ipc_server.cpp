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

  /**
   * Helper for handling json messages
   */
  class IPCServer::JSON
  {
    public:

      JSON(QString data)
      {
        _json = _engine.evaluate("("+data+")");

        if( !_json.isValid() )
          throw std::runtime_error("Failed to evaluted received data.");

        if( _engine.hasUncaughtException() )
          throw std::runtime_error("Failed to evaluted received data: "
                                   + _json.toString().toStdString());

        if( _json.isArray() || !_json.isObject() )
          throw std::runtime_error("Received data is no JSON object.");


//        QScriptValueIterator it(_json);
//        while( it.hasNext() )
//        {
//          it.next();
//          const std::string key = it.name().toStdString();
//          const auto val_config = types.find(key);
//
//          if( val_config == types.end() )
//          {
//            LOG_WARN("Unknown parameter (" << key << ")");
//            continue;
//          }
//
//          const std::string type = std::get<0>(val_config->second);
//          if(    ((type == "int" || type == "float") && !it.value().isNumber())
//              || (type == "string" && !it.value().isString()) )
//          {
//            LOG_WARN("Wrong type for parameter `" << key << "` ("
//                     << type << " expected)");
//            continue;
//          }
//
//          std::cout << "valid: "
//                    << key
//                    << "="
//                    << it.value().toString().toStdString()
//                    << std::endl;
//        }
      }

      template <class T>
      T getValue(const QString& key)
      {
        QScriptValue prop = _json.property(key);

        if( !prop.isValid() || prop.isUndefined() )
          throw std::runtime_error("No such property ("+key.toStdString()+")");

        return extractValue<T>(prop);
      }

      bool isSet(const QString& key)
      {
        QScriptValue prop = _json.property(key);
        return prop.isValid() && !prop.isUndefined();
      }

    private:

      template <class T> T extractValue(QScriptValue&);

      QScriptEngine _engine;
      QScriptValue  _json;
  };

  template<>
  QString IPCServer::JSON::extractValue(QScriptValue& val)
  {
    if( !val.isString() )
      throw std::runtime_error("Not a string");
    return val.toString();
  }

  template<>
  uint32_t IPCServer::JSON::extractValue(QScriptValue& val)
  {
    if( !val.isNumber() )
      throw std::runtime_error("Not a number");
    return val.toUInt32();
  }

  template<>
  QVariantList IPCServer::JSON::extractValue(QScriptValue& val)
  {
    if( !val.isArray() )
      throw std::runtime_error("Not an array");
    return val.toVariant().toList();
  }

  //----------------------------------------------------------------------------
  IPCServer::IPCServer()
  {

  }

  //----------------------------------------------------------------------------
  IPCServer::~IPCServer()
  {

  }

  //----------------------------------------------------------------------------
  void IPCServer::publishSlots(SlotCollector& slot_collector)
  {
    _slot_search_id = slot_collector.create<std::string>("/links[0]/id");
    _slot_search_stamp = slot_collector.create<uint32_t>("/links[0]/stamp");
    _slot_search_regions = slot_collector.create<std::vector<SlotType::Polygon>>
                                                        ("/links[0]/regions");
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

    try
    {
      JSON msg(data);
      QString task = msg.getValue<QString>("task");

      if( task == "INITIATE" )
      {
        QString id = msg.getValue<QString>("id");
        uint32_t stamp = msg.getValue<uint32_t>("stamp");

        *_slot_search_id->_data = id.toStdString();
        *_slot_search_stamp->_data = stamp;

        _slot_search_id->setValid(true);
        _slot_search_stamp->setValid(true);

        QString request(
          "{"
            "\"task\": \"REQUEST\","
            "\"id\": \"" + id.replace('"', "\\\"") + "\","
            "\"stamp\": " + QString("%1").arg(stamp) +
          "}"
        );

        for(auto socket = _clients.begin(); socket != _clients.end(); ++socket)
          (*socket)->write(request);

        _slot_search_regions->_data->clear();
        _slot_search_regions->setValid(true);

        parseRegions(msg);

        LOG_INFO("Received INITIATE: " << id.toStdString());
      }
      else if( task == "FOUND" )
      {
        QString id = msg.getValue<QString>("id");
        uint32_t stamp = msg.getValue<uint32_t>("stamp");

        if(    !_slot_search_id->isValid()
            || !_slot_search_stamp->isValid()
            || *_slot_search_id->_data != id.toStdString()
            || *_slot_search_stamp->_data != stamp )
        {
          LOG_WARN("Received FOUND for not wrong REQUEST");
          return;
        }

        LOG_INFO("Received FOUND: " << id.toStdString());
      }
    }
    catch(std::runtime_error& ex)
    {
      LOG_WARN("Failed to parse message: " << ex.what());
    }

//    std::map<std::string, std::tuple<std::string>> types
//    {
//      {"x", std::make_tuple("int")},
//      {"y", std::make_tuple("int")},
//      {"key", std::make_tuple("string")}
//    };
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

  //----------------------------------------------------------------------------
  void IPCServer::parseRegions(IPCServer::JSON& json)
  {
    if( !json.isSet("regions") )
      return;

    QVariantList regions = json.getValue<QVariantList>("regions");
    for(auto region = regions.begin(); region != regions.end(); ++region)
    {
      SlotType::Polygon polygon;
      auto regionlist = region->toList();
      for(auto point = regionlist.begin(); point != regionlist.end(); ++point)
      {
        QVariantList coords = point->toList();
        if( coords.size() != 2 )
        {
          LOG_WARN("Wrong coord count for point.");
          continue;
        }

        polygon.points.push_back(
          SlotType::Polygon::Point(
            coords.at(0).toInt(),
            coords.at(1).toInt()
            )
          );
      }

      if( !polygon.points.empty() )
        _slot_search_regions->_data->push_back(polygon);
    }
  }

} // namespace LinksRouting
