/*!
 * @file ipc_server.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#include "ipc_server.hpp"
#include "log.hpp"

#include <QMutex>
#include <QScriptEngine>
#include <QScriptValueIterator>

#include <algorithm>
#include <map>
#include <tuple>

namespace LinksRouting
{
  using namespace std::placeholders;

  WId windowAt( const WindowRegions& regions,
                const QPoint& pos );

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
  IPCServer::IPCServer( QMutex* mutex,
                        QWaitCondition* cond_data,
                        QWidget* widget ):
    Configurable("QtWebsocketServer"),
    _window_monitor(widget, std::bind(&IPCServer::regionsChanged, this, _1)),
    _mutex_slot_links(mutex),
    _cond_data_ready(cond_data)
  {
    assert(widget);
    registerArg("DebugRegions", _debug_regions);

//    qRegisterMetaType<WindowRegions>("WindowRegions");
//    assert( connect( &_window_monitor, SIGNAL(regionsChanged(WindowRegions)),
//             this,             SLOT(regionsChanged(WindowRegions)) ) );
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
  void IPCServer::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_routing =
      slot_subscriber.getSlot<SlotType::ComponentSelection>("/routing");
    _subscribe_user_config =
      slot_subscriber.getSlot<LinksRouting::Config*>("/user-config");
    assert(_subscribe_user_config->_data.get());
    _subscribe_mouse =
      slot_subscriber.getSlot<LinksRouting::SlotType::MouseEvent>("/mouse");
    _subscribe_popups =
      slot_subscriber.getSlot<SlotType::TextPopup>("/popups");
  }

  //----------------------------------------------------------------------------
  bool IPCServer::startup(Core* core, unsigned int type)
  {
    _window_monitor.start();
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
  void IPCServer::process(unsigned int type)
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

    _clients[ client_socket ] = ClientInfo();

    LOG_INFO(   "Client connected: "
             << client_socket->peerAddress().toString().toStdString()
             << ":" << client_socket->peerPort() );
  }

  //----------------------------------------------------------------------------
  void IPCServer::onDataReceived(QString data)
  {
    auto client = _clients.find(qobject_cast<QWsSocket*>(sender()));
    if( client == _clients.end() )
    {
      LOG_WARN("Received message from unknown client: " << data.toStdString());
      return;
    }

    ClientInfo& client_info = client->second;
    WId& client_wid = client_info.wid;

    std::cout << "Received (" << client_wid << "): "
              << data.toStdString()
              << std::endl;

    try
    {
      JSON msg(data);

      QString task = msg.getValue<QString>("task");
      if( task == "REGISTER" || task == "RESIZE" )
      {
        if( task == "REGISTER" )
        {
          QVariantList pos_list = msg.getValue<QVariantList>("pos");
          if( pos_list.length() != 2 )
          {
            LOG_WARN("Received invalid position (REGISTER)");
            return;
          }

          QPoint pos(pos_list.at(0).toInt(), pos_list.at(1).toInt());
          client_wid = windowAt(_window_monitor.getWindows(), pos);
        }

        QVariantList region_list = msg.getValue<QVariantList>("region");
        if( region_list.length() != 4 )
        {
          LOG_WARN("Received invalid region");
          return;
        }

        client_info.region.setRect
        (
          region_list.at(0).toInt(),
          region_list.at(1).toInt(),
          region_list.at(2).toInt(),
          region_list.at(3).toInt()
        );

        return;
      }

      QString id = msg.getValue<QString>("id").toLower(),
              title = msg.getValue<QString>("title", "");
      const std::string& id_str = id.toStdString();

      if( task == "GET" )
      {
        if( id == "/routing" )
        {
          QString routers = "{\"active\": \""
                          + QString::fromStdString(_subscribe_routing->_data->active)
                          + "\", \"default\": \""
                          + QString::fromStdString(_subscribe_routing->_data->getDefault())
                          + "\", \"available\":[";
          if( !_subscribe_routing->_data->available.empty() )
          {
            for( auto comp = _subscribe_routing->_data->available.begin();
                 comp != _subscribe_routing->_data->available.end();
                 ++comp )
              routers += "[\"" + QString::fromStdString(comp->first) + "\","
                       + (comp->second ? '1' : '0') + "],";
            routers.replace(routers.length() - 1, 1, ']');
          }
          else
            routers.push_back(']');
          routers.push_back('}');

          client->first->write
          ("{"
              "\"task\": \"GET-FOUND\","
              "\"id\": \"" + id.replace('"', "\\\"") + "\","
              "\"val\":" + routers +
           "}");
        }
        else
        {
          LOG_WARN("Requesting unknown value: " << id_str);
        }
        return;
      }
      else if( task == "SET" )
      {
        if( id == "/routing" )
        {
          _subscribe_routing->_data->request =
            msg.getValue<QString>("val").toStdString();
          for( auto link = _slot_links->_data->begin();
                   link != _slot_links->_data->end();
                   ++link )
            link->_stamp += 1;
          LOG_INFO("Trigger reroute -> routing algorithm changed");
          _cond_data_ready->wakeAll();
        }
        else if( id == "/config" )
        {
          (*_subscribe_user_config->_data)->setParameter
          (
            msg.getValue<QString>("var").toStdString(),
            msg.getValue<QString>("val").toStdString(),
            msg.getValue<QString>("type").toStdString()
          );
        }
        else
          LOG_WARN("Request setting unknown value: " << id_str);

        return;
      }

      uint32_t stamp = msg.getValue<uint32_t>("stamp");
      QMutexLocker lock_links(_mutex_slot_links);

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

        // TODO keep working for multiple links at the same time
        _subscribe_mouse->_data->clear();
        _subscribe_popups->_data->popups.clear();

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
        auto hedge = std::make_shared<LinkDescription::HyperEdge>();
        hedge->addNode( parseRegions(msg, client_wid) );
        updateCenter(hedge.get());

        _slot_links->_data->push_back(
          LinkDescription::LinkDescription
          (
            id_str,
            stamp,
            hedge, // TODO add props?
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
          socket->first->write(request);
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

        link->_link->addNode( parseRegions(msg, client_wid) );
        updateCenter(link->_link.get());
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
      else
      {
        LOG_WARN("Unknown task: " << task.toStdString());
        return;
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
    _cond_data_ready->wakeAll();
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

    _clients.erase(socket);
    socket->deleteLater();

    LOG_INFO("Client disconnected");
  }

  //----------------------------------------------------------------------------
  void IPCServer::regionsChanged(const WindowRegions& regions)
  {
    _mutex_slot_links->lock();
    _subscribe_mouse->_data->clear();
    _subscribe_popups->_data->popups.clear();

    bool need_update = false;
    for( auto link = _slot_links->_data->begin();
         link != _slot_links->_data->end();
         ++link )
    {
      if( updateHedge(regions, link->_link.get()) )
      {
        link->_stamp += 1;
        need_update = true;
      }
    }
    _mutex_slot_links->unlock();

    if( !need_update )
      return;

    LOG_INFO("Windows changed -> trigger reroute");
    _cond_data_ready->wakeAll();
  }

  //----------------------------------------------------------------------------
  class CoverWindows
  {
    public:
      CoverWindows( const WindowRegions::const_iterator& begin,
                    const WindowRegions::const_iterator& end ):
        _begin(begin),
        _end(end)
      {}

      bool hit(const QPoint& point)
      {
        for(auto it = _begin; it != _end; ++it)
          if( it->region.contains(point) )
            return true;
        return false;
      }

      LinkDescription::NodePtr getNearestVisible( const float2& point,
                                                  bool triangle = false )
      {
        const QRect& reg = _begin->region;

        float dist[4] = {
          point.x - reg.left(),
          reg.right() - point.x,
          point.y - reg.top(),
          reg.bottom() - point.y
        };

        float min_dist = dist[0];
        int min_index = 0;

        for( size_t i = 1; i < 4; ++i )
          if( dist[i] < min_dist )
          {
            min_dist = dist[i];
            min_index = i;
          }

        float2 new_center = point,
               normal(0,0);
        int sign = (min_index & 1) ? 1 : -1;
        min_dist *= sign;

        if( min_index < 2 )
        {
          new_center.x += min_dist;
          normal.x = sign;
        }
        else
        {
          new_center.y += min_dist;
          normal.y = sign;
        }

        LinkDescription::points_t link_points;
        link_points.push_back(new_center += 12 * normal);

        LinkDescription::points_t points;

        if( triangle )
        {
          points.push_back(new_center += 6 * normal.normal());
          points.push_back(new_center -= 6 * normal.normal() + 12 * normal);
          points.push_back(new_center -= 6 * normal.normal() - 12 * normal);
        }
        else
        {
          points.push_back(new_center += 0.5 * normal.normal());
          points.push_back(new_center -= 9 * normal);

          points.push_back(new_center += 2.5 * normal.normal());
          points.push_back(new_center += 6   * normal.normal() + 3 * normal);
          points.push_back(new_center += 1.5 * normal.normal() - 3 * normal);
          points.push_back(new_center -= 6   * normal.normal() + 3 * normal);
          points.push_back(new_center -= 9   * normal.normal());
          points.push_back(new_center -= 6   * normal.normal() - 3 * normal);
          points.push_back(new_center += 1.5 * normal.normal() + 3 * normal);
          points.push_back(new_center += 6   * normal.normal() - 3 * normal);
          points.push_back(new_center += 2.5 * normal.normal());

          points.push_back(new_center += 9 * normal);
        }

        auto node = std::make_shared<LinkDescription::Node>(points, link_points);
        node->set("virtual-covered", true);
        //node->set("filled", true);

        return node;
      }

    private:
      const WindowRegions::const_iterator& _begin,
                                           _end;
  };

  //----------------------------------------------------------------------------
  bool IPCServer::updateHedge( const WindowRegions& regions,
                               LinkDescription::HyperEdge* hedge )
  {
    WId client_wid =
#ifdef _WIN32
      reinterpret_cast<WId>
      (
#endif
        hedge->get<unsigned long>("client_wid", 0)
#ifdef _WIN32
      )
#endif
    ;

    auto client_info = std::find_if
    (
      _clients.begin(),
      _clients.end(),
      [&client_wid](const ClientInfos::value_type& it)
      {
        return it.second.wid == client_wid;
      }
    );
    QRect region;
    WindowRegions::const_iterator first_above = regions.end();
    if( client_info != _clients.end() )
    {
      region = client_info->second.region;

      if( client_wid )
      {
        auto window_info = std::find_if
        (
          regions.begin(),
          regions.end(),
          [&client_wid](const WindowInfo& winfo)
          {
            return winfo.id == client_wid;
          }
        );
        region.translate( window_info->region.topLeft() );
        first_above = window_info + 1;
      }
    }

    bool modified = false;

    struct OutsideScroll
    {
      float2 pos;
      float2 normal;
      size_t num_outside;
    } outside_scroll[] = {
      {float2(0, region.top()),    float2( 0, 1), 0},
      {float2(0, region.bottom()), float2( 0,-1), 0},
      {float2(region.left(),  0),  float2( 1, 0), 0},
      {float2(region.right(), 0),  float2(-1, 0), 0}
    };
    const size_t num_outside_scroll = sizeof(outside_scroll)
                                    / sizeof(outside_scroll[0]);

//    std::vector<LinkDescription::nodes_t::iterator> old_outside_nodes;

    for( auto node = hedge->getNodes().begin();
              node != hedge->getNodes().end();
            ++node )
      if( (*node)->get<bool>("virtual-covered", false) )
        node = hedge->getNodes().erase(node);
    // TODO check why still sometimes regions don't get removed immediately

    float2 hedge_center;
    size_t num_visible = 0;

    for( auto node = hedge->getNodes().begin();
              node != hedge->getNodes().end();
            ++node )
    {
      if(    !(*node)->get<std::string>("virtual-outside").empty()
          || (*node)->get<bool>("virtual-covered", false) )
      {
        //old_outside_nodes.push_back(node);
        node = hedge->getNodes().erase(node);
        modified = true;
        continue;
      }

      if( (*node)->get<bool>("outside", false) )
      {
        (*node)->set("hidden", true);

        float2 center = (*node)->getCenter();
        if( center != float2(0,0) )
        {
          for( size_t i = 0; i < num_outside_scroll; ++i )
          {
            OutsideScroll& out = outside_scroll[i];
            if( (center - out.pos) * out.normal > 0 )
              continue;

            if( out.normal.x == 0 )
              out.pos.x += center.x;
            else
              out.pos.y += center.y;
            out.num_outside += 1;
          }
        }
        continue;
      }

      for(auto child = (*node)->getChildren().begin();
               child != (*node)->getChildren().end();
             ++child )
      {
        if( updateHedge(regions, child->get()) )
          modified = true;

        float2 center = (*child)->getCenter();
        if( center != float2(0,0) )
        {
          hedge_center += center;
          num_visible += 1;
        }
      }

      if( !client_wid )
        continue;

      if( updateRegion(regions, node->get(), client_wid) )
        modified = true;
    }

    for( size_t i = 0; i < num_outside_scroll; ++i )
    {
      OutsideScroll& out = outside_scroll[i];
      if( !out.num_outside )
        continue;

      std::cout << i << ": " << out.num_outside << std::endl;

      if( out.normal.x == 0 )
        out.pos.x /= out.num_outside;
      else
        out.pos.y /= out.num_outside;

      LinkDescription::points_t link_points;
      link_points.push_back(out.pos += 5 * out.normal);

      LinkDescription::points_t points;
      points.push_back(out.pos +=  8 * out.normal.normal());
      points.push_back(out.pos -=  5 * out.normal);
      points.push_back(out.pos -= 16 * out.normal.normal());
      points.push_back(out.pos +=  5 * out.normal);

      auto node = std::make_shared<LinkDescription::Node>(points, link_points);
      node->set("virtual-outside", "side[" + std::to_string(static_cast<unsigned long long>(i)) + "]");
      node->set("filled", true);
      updateRegion(regions, node.get(), client_wid);

      QRect reg(points[0].x, points[0].y, 0, 0);
      const int border = 4;
      for(size_t j = 1; j < points.size(); ++j)
      {
        int x = points[j].x,
            y = points[j].y;

        if( x - border < reg.left() )
          reg.setLeft(x - border);
        if( x + border > reg.right() )
          reg.setRight(x + border);

        if( y - border < reg.top() )
          reg.setTop(y - border);
        else if( y + border > reg.bottom() )
          reg.setBottom(y + border);
      }

      {
        std::string text = std::to_string(static_cast<unsigned long long>(out.num_outside));
        int width = text.length() * 10 + 10;

        SlotType::TextPopup::Popup popup = {
          text,
          float2(reg.center().x() - width/2, reg.top() - 20),
          float2(width, 20),
          false
        };
        size_t index = _subscribe_popups->_data->popups.size();
        _subscribe_popups->_data->popups.push_back(popup);

        _subscribe_mouse->_data->_move_callbacks.push_back(
          [&,index,reg](int x, int y)
          {
            auto& popup = _subscribe_popups->_data->popups[index];
            if( reg.contains(x, y) )
            {
              if( popup.visible )
                return;

              popup.visible = true;
            }
            else if( !popup.visible )
              return;
            else
              popup.visible = false;

            _cond_data_ready->wakeAll();
          }
        );
      }

      hedge->getNodes().push_back( node );
    }

    for( auto node = hedge->getNodes().begin();
              node != hedge->getNodes().end();
            ++node )
      if( (*node)->get<bool>("virtual-covered", false) )
        node = hedge->getNodes().erase(node);

    if( first_above != regions.end() )
    {
      CoverWindows cover_windows(first_above, regions.end());

      // Show regions covered by other windows
      for( auto node = hedge->getNodes().begin();
                node != hedge->getNodes().end();
              ++node )
      {
        bool covered = !(*node)->get<bool>("outside", false)
                    &&  (*node)->get<bool>("hidden", false);
        (*node)->set("covered", covered);

        if( !covered )
          continue;

        hedge->getNodes().push_back(
          cover_windows.getNearestVisible((*node)->getCenter())
        );
      }
    }

    for( auto node = hedge->getNodes().begin();
              node != hedge->getNodes().end();
            ++node )
      if( !(*node)->get<bool>("hidden") )
      {
        float2 center = (*node)->getCenter();
        if( center != float2(0,0) )
        {
          hedge_center += center;
          num_visible += 1;
        }
      }

    if( num_visible > 0 )
      hedge_center /= num_visible;

    hedge->setCenter(hedge_center);

    return modified;
  }

  //----------------------------------------------------------------------------
  bool IPCServer::updateCenter(LinkDescription::HyperEdge* hedge)
  {
    float2 hedge_center;
    size_t num_visible = 0;

    for( auto node = hedge->getNodes().begin();
              node != hedge->getNodes().end();
            ++node )
    {
      for(auto child = (*node)->getChildren().begin();
               child != (*node)->getChildren().end();
             ++child )
      {
        float2 center = (*child)->getCenter();
        if( center != float2(0,0) )
        {
          hedge_center += center;
          num_visible += 1;
        }
      }
    }

    if( !num_visible )
      return false;

    hedge->setCenter(hedge_center / num_visible);

    return true;
  }

  //----------------------------------------------------------------------------
  bool IPCServer::updateRegion( const WindowRegions& regions,
                                LinkDescription::Node* node,
                                WId client_wid )
  {
    size_t hidden_count = 0;
    for( auto vert = node->getVertices().begin();
              vert != node->getVertices().end();
            ++vert )
    {
      if( client_wid != windowAt(regions, QPoint(vert->x, vert->y)) )
        ++hidden_count;
    }

    LinkDescription::props_t& props = node->getProps();
    LinkDescription::props_t::iterator hidden = props.find("hidden");
    if( hidden_count > node->getVertices().size() / 2 )
    {
      if( hidden == props.end() )
      {
        // if more than half of the points are in another window don't show
        // region
        props["hidden"] = "true";
        return true;
      }
    }
    else
    {
      if( hidden != props.end() )
      {
        props.erase(hidden);
        return true;
      }
    }
    return false;
  }

  //----------------------------------------------------------------------------
  LinkDescription::NodePtr IPCServer::parseRegions( IPCServer::JSON& json,
                                                    WId client_wid )
  {
    std::cout << "Parse regions (" << client_wid << ")" << std::endl;

    const WindowRegions windows = _window_monitor.getWindows();
    LinkDescription::nodes_t nodes;

    if( !json.isSet("regions") )
      return LinkDescription::NodePtr();

    QVariantList regions = json.getValue<QVariantList>("regions");
    for(auto region = regions.begin(); region != regions.end(); ++region)
    {
      LinkDescription::points_t points;
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

          points.push_back(float2(
            coords.at(0).toInt(),
            coords.at(1).toInt()
          ));
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

      if( points.empty() )
        continue;

      nodes.push_back( std::make_shared<LinkDescription::Node>(points, props) );
    }

    LinkDescription::HyperEdgePtr hedge =
      std::make_shared<LinkDescription::HyperEdge>(nodes);
    hedge->set("client_wid", std::to_string(
#ifdef _WIN32
        reinterpret_cast<unsigned long long>(client_wid)
#else
        client_wid
#endif
    ));
    updateHedge(windows, hedge.get());

    return std::make_shared<LinkDescription::Node>(hedge);
  }

  //----------------------------------------------------------------------------
  WId windowAt( const WindowRegions& regions,
                const QPoint& pos )
  {
    for( auto reg = regions.rbegin(); reg != regions.rend(); ++reg )
    {
      if( reg->region.contains(pos) )
        return reg->id;
    }
    return 0;
  }

} // namespace LinksRouting
