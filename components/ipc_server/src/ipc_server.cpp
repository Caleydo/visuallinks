/*!
 * @file ipc_server.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#include "ipc_server.hpp"
#include "log.hpp"

#include <QxtGui/qxtwindowsystem.h>

#include <QMutex>
#include <QScriptEngine>
#include <QScriptValueIterator>

#include <algorithm>
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

      template <class T>
      T getValue(const QString& key, const T& def)
      {
        QScriptValue prop = _json.property(key);

        if( !prop.isValid() || prop.isUndefined() )
          return def;

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
  IPCServer::IPCServer(QMutex* mutex, QWidget* widget):
    _mutex_slot_links(mutex),
    _widget(widget)
  {
    assert(widget);
    registerArg("DebugRegions", _debug_regions);
  }

  //----------------------------------------------------------------------------
  IPCServer::~IPCServer()
  {

  }

  //----------------------------------------------------------------------------
  void IPCServer::publishSlots(SlotCollector& slot_collector)
  {
    _slot_links = slot_collector.create<LinkDescription::LinkList>("/links");
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
    if( !_debug_regions.empty() )
    {
      onDataReceived( QString::fromStdString(_debug_regions) );
      _debug_regions.clear();
    }
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
WId cid = 0;
  //----------------------------------------------------------------------------
  void IPCServer::onDataReceived(QString data)
  {
    std::cout << "Received: " << data.toStdString() << std::endl;

    try
    {
      JSON msg(data);

      QString task = msg.getValue<QString>("task");
      if( task == "REGISTER" )
      {
        QVariantList pos = msg.getValue<QVariantList>("pos");
        if( pos.length() != 2 )
        {
          LOG_WARN("Received invalid position (REGISTER)");
          return;
        }

        cid = windowAt(QPoint(pos.at(0).toInt(), pos.at(1).toInt()));
        return;
      }

      QString id = msg.getValue<QString>("id").toLower(),
              title = msg.getValue<QString>("title", "");
      uint32_t stamp = msg.getValue<uint32_t>("stamp");
      QMutexLocker lock_links(_mutex_slot_links);

      const std::string& id_str = id.toStdString();
      auto link = std::find_if
      (
        _slot_links->_data->begin(),
        _slot_links->_data->end(),
        [&id_str](const LinkDescription::LinkDescription& desc)
        {
          return desc._id == id_str;
        }
      );

      if( task == "INITIATE" )
      {
        LOG_INFO("Received INITIATE: " << id_str);
        uint32_t color_id = 0;

        // Remove eventually existing search for same id
        if( link != _slot_links->_data->end() )
        {
          // keep color
          color_id = link->_color_id;

          LOG_INFO("Replacing search for " << link->_id);
          _slot_links->_data->erase(link);
        }
        else
        {
          // get first unused color
          while
          (
            std::find_if
            (
              _slot_links->_data->begin(),
              _slot_links->_data->end(),
              [color_id](const LinkDescription::LinkDescription& desc)
              {
                return desc._color_id == color_id;
              }
            ) != _slot_links->_data->end()
          )
            ++color_id;
        }

        _slot_links->_data->push_back(
          LinkDescription::LinkDescription
          (
            id_str,
            stamp,
            LinkDescription::HyperEdge( parseRegions(msg, title) ), // TODO add props?
            color_id
          )
        );
        _slot_links->setValid(true);

        // Send request to all clients
        QString request(
          "{"
            "\"task\": \"REQUEST\","
            "\"id\": \"" + id.replace('"', "\\\"") + "\","
            "\"stamp\": " + QString("%1").arg(stamp) +
          "}"
        );

//        for(QWsSocket* socket: _clients)
//          socket->write(request);
        for(auto socket = _clients.begin(); socket != _clients.end(); ++socket)
          (*socket)->write(request);
      }
      else if( task == "FOUND" )
      {
        if( link == _slot_links->_data->end() )
        {
          LOG_WARN("Received FOUND for none existing REQUEST");
          return;
        }

        if( link->_stamp != stamp )
        {
          LOG_WARN("Received FOUND for wrong REQUEST (different stamp)");
          return;
        }

        LOG_INFO("Received FOUND: " << id_str);

        link->_link.addNodes( parseRegions(msg, title) );
      }
      else if( task == "ABORT" )
      {
        if( id.isEmpty() && stamp == (uint32_t)-1 )
          _slot_links->_data->clear();
        else if( link == _slot_links->_data->end() )
          LOG_WARN("Received ABORT for none existing REQUEST");
        else
        {
          _slot_links->_data->erase(link);

//          for(auto socket = _clients.begin(); socket != _clients.end(); ++socket)
//            (*socket)->write(request);
        }
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
  std::vector<LinkDescription::Node>
  IPCServer::parseRegions(IPCServer::JSON& json, const QString& window_title)
  {
    std::cout << "Parse regions: (" << cid << ") "
                                    << window_title.toStdString() << std::endl;

    WindowList windows = QxtWindowSystem::windows();
    std::vector<LinkDescription::Node> nodes;

    if( !json.isSet("regions") )
      return nodes;

    QVariantList regions = json.getValue<QVariantList>("regions");
    for(auto region = regions.begin(); region != regions.end(); ++region)
    {
      std::vector<float2> points;
      LinkDescription::props_t props;

      //for(QVariant point: region.toList())
      auto regionlist = region->toList();
      for(auto point = regionlist.begin(); point != regionlist.end(); ++point)
      {
        if( point->type() == QVariant::List )
        {
          QVariantList coords = point->toList();
          if( coords.size() != 2 )
          {
            LOG_WARN("Wrong coord count for point.");
            continue;
          }

          float2 point(
            coords.at(0).toInt(),
            coords.at(1).toInt()
          );

          WId wid = 0;
          QPoint pos(point.x, point.y);
          for (int i = windows.size() - 1; i >= 0; --i)
          {
              WId wid_cur = windows.at(i);
              if( wid_cur == _widget->winId() )
                // Ignore own window as it's always on top, but transparent
                continue;
              else if( QxtWindowSystem::windowGeometry(wid_cur).contains(pos) )
              {
                  wid = wid_cur;
                  break;
              }
          }

          if( wid != cid )
          {
            // if point is in other window don't show region
            points.clear();
            break;
          }

          points.push_back(point);
        }
        else if( point->type() == QVariant::Map )
        {
          auto map = point->toMap();
          for( auto it = map.constBegin(); it != map.constEnd(); ++it )
            props[ it.key().toStdString() ] = it->toString().toStdString();
        }
        else
          LOG_WARN("Wrong data type: " << point->typeName());
      }

      if( !points.empty() )
        nodes.push_back( LinkDescription::Node(points, props) );
    }

    return nodes;
  }

  //----------------------------------------------------------------------------
  WId IPCServer::windowAt(const QPoint& pos) const
  {
    WindowList list = QxtWindowSystem::windows();
    for (int i = list.size() - 1; i >= 0; --i)
    {
        WId wid = list.at(i);
        if(    wid != _widget->winId()
            && QxtWindowSystem::windowGeometry(wid).contains(pos) )
          return wid;
    }
    return 0;
  }

} // namespace LinksRouting
