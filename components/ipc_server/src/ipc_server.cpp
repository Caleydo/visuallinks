/*!
 * @file ipc_server.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#include "ipc_server.hpp"
#include "log.hpp"
#include "ClientInfo.hxx"
#include "JSONParser.h"
#include "common/PreviewWindow.hpp"

#include <QMutex>
#include <QWebSocket>

#include <algorithm>
#include <map>
#include <tuple>
#include <limits>

namespace LinksRouting
{
  using namespace std::placeholders;

  std::string to_string(const QRect& r)
  {
    std::stringstream strm;
    strm << r.left() << ' ' << r.top() << ' ' << r.right() << ' ' << r.bottom();
    return strm.str();
  }

  std::string to_string(const Rect& r)
  {
    return to_string( r.toQRect() );
  }

  template<typename T>
  void clampedStep( T& val,
                    T step,
                    T min,
                    T max,
                    T max_step = 1)
  {
    step = std::max(-max_step, std::min(step, max_step));
    clamp<T>(val += step, min, max);
  }

  class IPCServer::TileHandler:
    public SlotType::TileHandler
  {
    public:
      TileHandler(IPCServer* ipc_server);

      virtual bool updateRegion( SlotType::TextPopup::Popup& popup,
                                 float2 center = float2(-9999, -9999),
                                 float2 rel_pos = float2() );
      virtual bool updateRegion( SlotType::XRayPopup::HoverRect& popup );

    protected:
      friend class IPCServer;
      IPCServer* _ipc_server;

      struct TileRequest
      {
        QWebSocket* socket;
        HierarchicTileMapWeakPtr tile_map;
        int zoom;
        size_t x, y;
        float2 tile_size;
        clock::time_point time_stamp;
      };

      typedef std::map<uint8_t, TileRequest> TileRequests;
      TileRequests  _tile_requests;
      uint8_t       _tile_request_id;
      int           _new_request;

      bool updateTileMap( const HierarchicTileMapPtr& tile_map,
                          QWebSocket* socket,
                          const Rect& rect,
                          int zoom );
  };

  //----------------------------------------------------------------------------
  IPCServer::IPCServer( QMutex* mutex,
                        QWaitCondition* cond_data ):
    Configurable("QtWebsocketServer"),
    _window_monitor(std::bind(&IPCServer::regionsChanged, this, _1)),
    _mutex_slot_links(mutex),
    _cond_data_ready(cond_data),
    _dirty_flags(0)
  {
    //assert(widget);
    registerArg("DebugRegions", _debug_regions);
    registerArg("DebugFullPreview", _debug_full_preview_path);
    registerArg("PreviewWidth", _preview_width = 800);
    registerArg("PreviewHeight", _preview_height = 400);
    registerArg("PreviewAutoWidth", _preview_auto_width = true);
    registerArg("OutsideSeeThrough", _outside_see_through = true);
  }

  //----------------------------------------------------------------------------
  IPCServer::~IPCServer()
  {

  }

  //----------------------------------------------------------------------------
  void IPCServer::publishSlots(SlotCollector& slot_collector)
  {
    _slot_links = slot_collector.create<LinkDescription::LinkList>("/links");
    _slot_xray = slot_collector.create<SlotType::XRayPopup>("/xray");
    _slot_outlines =
      slot_collector.create<SlotType::CoveredOutline>("/covered-outlines");
    _slot_tile_handler =
      slot_collector.create< SlotType::TileHandler,
                             TileHandler,
                             IPCServer*
                           >("/tile-handler", this);
    _tile_handler = dynamic_cast<TileHandler*>(_slot_tile_handler->_data.get());
    assert(_tile_handler);
  }

  //----------------------------------------------------------------------------
  void IPCServer::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_routing =
      slot_subscriber.getSlot<SlotType::ComponentSelection>("/routing");
    _subscribe_user_config =
      slot_subscriber.getSlot<LinksRouting::Config*>("/user-config");
    assert(_subscribe_user_config->_data.get());

    _subscribe_desktop_rect =
      slot_subscriber.getSlot<Rect>("/desktop/rect");

    _subscribe_mouse =
      slot_subscriber.getSlot<LinksRouting::SlotType::MouseEvent>("/mouse");
    _subscribe_popups =
      slot_subscriber.getSlot<SlotType::TextPopup>("/popups");
    _subscribe_previews =
      slot_subscriber.getSlot<SlotType::Preview>("/previews");

    _subscribe_mouse->_data->_click_callbacks.push_back
    (
      std::bind(&IPCServer::onClick, this, _1, _2)
    );
    _subscribe_mouse->_data->_move_callbacks.push_back
    (
      std::bind(&IPCServer::onMouseMove, this, _1, _2)
    );
    _subscribe_mouse->_data->_scroll_callbacks.push_back
    (
      std::bind(&IPCServer::onScrollWheel, this, _1, _2, _3)
    );
    _subscribe_mouse->_data->_drag_callbacks.push_back
    (
      std::bind(&IPCServer::onDrag, this, _1)
    );
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
    _server = new QWebSocketServer(
      QStringLiteral("Hidden Content Server"),
      QWebSocketServer::NonSecureMode,
      this
    );
    if( !_server->listen(QHostAddress::Any, port) )
    {
      LOG_ERROR("Failed to start WebSocket server on port " << port << ": "
                << _server->errorString() );
      return;
    }

    LOG_INFO("WebSocket server listening on port " << port);
    connect( _server, &QWebSocketServer::newConnection,
             this, &IPCServer::onClientConnection );
  }

  //----------------------------------------------------------------------------
  void IPCServer::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  uint32_t IPCServer::process(unsigned int type)
  {
    if( !_debug_regions.empty() )
    {
      onTextReceived( QString::fromStdString(_debug_regions) );
      _debug_regions.clear();
    }

    if( !_debug_full_preview_path.empty() )
    {
      _full_preview_img =
        QGLWidget::convertToGLFormat
        (
          QImage( _debug_full_preview_path.c_str() ).mirrored(false, true)
        );
      _debug_full_preview_path.clear();
    }

    if( _tile_handler->_new_request > 0 )
      --_tile_handler->_new_request;
    else
    {
      int allowed_request = 6;
      for(auto req =  _tile_handler->_tile_requests.rbegin();
               req != _tile_handler->_tile_requests.rend();
             ++req )
      {
        if( !req->second.socket )
          continue;

        HierarchicTileMapPtr tile_map = req->second.tile_map.lock();
        if( !tile_map )
        {
          // TODO req = _tile_handler->_tile_requests.erase(req);
          continue;
        }

        float scale = 1/tile_map->getLayerScale(req->second.zoom);
        Rect src( float2( req->second.x * tile_map->getTileWidth(),
                          req->second.y * tile_map->getTileHeight() ),
                  req->second.tile_size );
        src *= scale;
        src.pos.x += tile_map->margin_left;

        std::cout << "request tile " << src.toString(true) << std::endl;
        req->second.socket->sendTextMessage(QString(
        "{"
          "\"task\": \"GET\","
          "\"id\": \"preview-tile\","
          "\"size\": [" + QString::number(req->second.tile_size.x)
                  + "," + QString::number(req->second.tile_size.y)
                  + "],"
          "\"sections_src\":" + to_string(tile_map->partitions_src).c_str() + ","
          "\"sections_dest\":" + to_string(tile_map->partitions_dest).c_str() + ","
          "\"src\": " + src.toString(true).c_str() + ","
          "\"req_id\": " + QString::number(req->first) +
        "}"));

        req->second.socket = 0;
        if( --allowed_request <= 0 )
          break;
      }
    }

    static clock::time_point last_time = clock::now();
    clock::time_point now = clock::now();
    auto dur_us = std::chrono::duration_cast<std::chrono::microseconds>
    (
      now - last_time
    ).count();
    double dt = dur_us / 1000000.;
    last_time = now;

    auto updatePopup = [&](SlotType::AnimatedPopup& popup) -> uint32_t
    {
      uint32_t flags = 0;

      bool visible = popup.isVisible();
      if( popup.update(dt) )
        flags |= RENDER_DIRTY;

      double alpha = popup.getAlpha();

      if(    visible != popup.isVisible()
          || (alpha > 0 && alpha < 0.5) )
        flags |= MASK_DIRTY;

      return flags;
    };

    foreachPopup
    (
      [&](SlotType::TextPopup::Popup& popup, QWebSocket&, ClientInfo&) -> bool
      {
        if( popup.node )
        {
          if( popup.node->get<bool>("hidden") )
            popup.region.hide();
          else if( !popup.text.empty() )
            popup.region.show();
        }
        _dirty_flags |= updatePopup(popup.hover_region);

        if( popup.hover_region.isVisible() )
        {
          if( !popup.preview )
            popup.preview = _subscribe_previews->_data->getWindow(&popup);

          popup.preview->update(dt);
        }
        else
        {
          if( popup.preview )
          {
            popup.preview->release();
            popup.preview = nullptr;
          }
        }

        return false;
      }
    );

    WId preview_wid = 0;
    Rect preview_region;
    foreachPreview
    (
      [&](SlotType::XRayPopup::HoverRect& preview, QWebSocket&, ClientInfo&)
         -> bool
      {
        _dirty_flags |= updatePopup(preview);
        if( preview.getAlpha() > (preview.isFadeIn() ? 0.9 : 0.3) )
        {
          preview_wid = preview.node->get<WId>("client_wid");
          preview_region = preview.preview_region;
        }
        preview.node->set("alpha", preview.getAlpha());

        if( preview.isVisible() )
        {
          if( !preview.preview )
            preview.preview = _subscribe_previews->_data->getWindow(&preview);

          preview.preview->update(dt);
        }
        else
        {
          if( preview.preview )
          {
            preview.preview->release();
            preview.preview = nullptr;
          }
        }

        return false;
      }
    );

    bool changed = false;
    for(auto& cinfo: _clients)
      changed |= cinfo.second->updateHoverCovered(preview_wid, preview_region);

    if( changed )
      _dirty_flags |= LINKS_DIRTY;

    uint32_t flags = _dirty_flags;
    _dirty_flags = 0;
    return flags;
  }

  //----------------------------------------------------------------------------
  bool IPCServer::getOutsideSeeThrough() const
  {
    return routingActive() && _outside_see_through;
  }

  //----------------------------------------------------------------------------
  IPCServer::PopupIterator
  IPCServer::addPopup( const ClientInfo& client_info,
                       const SlotType::TextPopup::Popup& popup )
  {
    auto& popups = _subscribe_popups->_data->popups;
    auto popup_it = popups.insert(popups.end(), popup);

    auto client = std::find_if
    (
      _clients.begin(),
      _clients.end(),
      [&client_info](const ClientInfos::value_type& c)
      {
        return c.second->getWindowInfo().id == client_info.getWindowInfo().id;
      }
    );

    if( client == _clients.end() )
      return popup_it;

    popup_it->client_socket = client->first;
    return popup_it;
  }

  //----------------------------------------------------------------------------
  void IPCServer::removePopup(const PopupIterator& popup)
  {
    _subscribe_popups->_data->popups.erase(popup);
  }

  //----------------------------------------------------------------------------
  void IPCServer::removePopups(const std::list<PopupIterator>& popups_remove)
  {
    auto& popups = _subscribe_popups->_data->popups;
    for(auto const& it: popups_remove)
      popups.erase(it);
  }

  //----------------------------------------------------------------------------
  IPCServer::XRayIterator
  IPCServer::addCoveredPreview( const std::string& link_id,
                                const ClientInfo& client_info,
                                const LinkDescription::NodePtr& node,
                                const HierarchicTileMapWeakPtr& tile_map,
                                const QRect& viewport,
                                const QRect& scroll_region,
                                bool extend )
  {
    QRect preview_region =
      scroll_region.intersected(extend ? desktopRect().toQRect() : viewport);
    QRect source_region
    (
      preview_region.topLeft() - scroll_region.topLeft(),
      preview_region.size()
    );

    node->set("covered-region", to_string(viewport));
    node->set("covered-preview-region", to_string(preview_region));
    node->setOrClear("hover", false);

    Rect bb;
    for( auto vert = node->getVertices().begin();
              vert != node->getVertices().end();
            ++vert )
      bb.expand(*vert);

    SlotType::XRayPopup::HoverRect xray;
    xray.link_id = link_id;
    xray.region = bb;
    xray.preview_region = preview_region;
    xray.source_region = source_region;
    xray.node = node;
    xray.tile_map = tile_map;
    xray.client_socket = getSocketByWId(client_info.getWindowInfo().id);

    auto& previews = _slot_xray->_data->popups;
    return previews.insert(previews.end(), xray);
  }

  //----------------------------------------------------------------------------
  void IPCServer::removeCoveredPreview(const XRayIterator& preview)
  {
    if( preview->node )
      preview->node->setOrClear("alpha", false);
    _slot_xray->_data->popups.erase(preview);
  }

  //----------------------------------------------------------------------------
  void IPCServer::removeCoveredPreviews(const std::list<XRayIterator>& previews)
  {
    auto& popups = _slot_xray->_data->popups;
    for(auto const& it: previews)
    {
      if( it->node )
        it->node->setOrClear("alpha", false);
      popups.erase(it);
    }
  }

  //----------------------------------------------------------------------------
  IPCServer::OutlineIterator
  IPCServer::addOutline(const ClientInfo& client_info)
  {
    auto const& winfo = client_info.getWindowInfo();
    SlotType::CoveredOutline::Outline outline;
    outline.title = to_string(winfo.title);
    outline.region_outline.pos = winfo.region.topLeft();
    outline.region_outline.size = winfo.region.size();
    outline.region_title.pos = winfo.region.topLeft() + float2(2, 5);
    outline.region_title.size.x = outline.title.size() * 7;
    outline.region_title.size.y = 20;
    outline.preview_valid = false;

    auto& outlines = _slot_outlines->_data->popups;
    return outlines.insert(outlines.end(), outline);
  }

  //----------------------------------------------------------------------------
  void IPCServer::removeOutline(const OutlineIterator& outline)
  {
    if( outline->preview_valid )
      removeCoveredPreview(outline->preview);
    _slot_outlines->_data->popups.erase(outline);
  }

  //----------------------------------------------------------------------------
  void IPCServer::removeOutlines(const std::list<OutlineIterator>& outlines_it)
  {
    auto& outlines = _slot_outlines->_data->popups;
    for(auto const& it: outlines_it)
    {
      if( it->preview_valid )
        removeCoveredPreview(it->preview);
      outlines.erase(it);
    }
  }

  //----------------------------------------------------------------------------
  bool IPCServer::routingActive() const
  {
    return _subscribe_routing->_data->active != "NoRouting";
  }

  //----------------------------------------------------------------------------
  void IPCServer::onClientConnection()
  {
    QWebSocket* client = _server->nextPendingConnection();

    connect(client, &QWebSocket::textMessageReceived, this, &IPCServer::onTextReceived);
    connect(client, &QWebSocket::binaryMessageReceived, this, &IPCServer::onBinaryReceived);
    connect(client, &QWebSocket::disconnected, this, &IPCServer::onClientDisconnection);

    _clients[ client ].reset(new ClientInfo(this));

    LOG_INFO( "Client connected: " << client->peerAddress().toString()
                                   << ":" << client->peerPort() );
  }

  //----------------------------------------------------------------------------
  void IPCServer::onTextReceived(QString data)
  {
    auto client = _clients.find(qobject_cast<QWebSocket*>(sender()));
    if( client == _clients.end() )
    {
      LOG_WARN("Received message from unknown client: " << data);
      return;
    }

    ClientInfo& client_info = *client->second.get();
    std::cout << "Received (" << client_info.getWindowInfo().id << "): "
              << data << std::endl;

    try
    {
      JSONParser msg(data);

      QString task = msg.getValue<QString>("task");

      {
        QMutexLocker lock_links(_mutex_slot_links);

        if( task == "REGISTER" || task == "RESIZE" )
        {
          const WindowRegions& windows = _window_monitor.getWindows();

          if( msg.hasChild("viewport") )
            client_info.viewport = msg.getValue<QRect>("viewport");

          if( task == "REGISTER" )
          {
            const QString title = msg.getValue<QString>("title", "");

            if(    msg.hasChild("pos")
                || msg.hasChild("pid")
                || !title.isEmpty() )
            {
              WId wid = 0;

              if( msg.hasChild("pos") )
                wid = windows.windowIdAt(msg.getValue<QPoint>("pos"));
              else if( msg.hasChild("pid") )
                wid = windows.findId(msg.getValue<uint32_t>("pid"), title);
              else
                wid = windows.findId(title);

              client_info.setWindowId(wid);
            }

            if( msg.hasChild("cmds") )
            {
              for(QString cmd: msg.getValue<QStringList>("cmds"))
                client_info.addCommand(cmd);
            }
          }
          client_info.update(windows);
          return;
        }
        else if( task == "SYNC" )
        {
          if(    msg.getValue<QString>("type") == "SCROLL"
              && msg.getValue<QString>("item", "CONTENT") == "CONTENT" )
            // TODO handle ELEMENT scroll?
          {
            client_info.setScrollPos( msg.getValue<QPoint>("pos") );
            client_info.update(_window_monitor.getWindows());

            // Forward scroll events to clients which support this (eg. for
            // synchronized scrolling)
//            for(auto const& socket: _clients)
//            {
//              if(    sender() != socket.first
//                  && socket.second->supportsCommand("scroll") )
//                socket.first->sendTextMessage(data);
//            }
            dirtyLinks();
          }

          return;
        }
        else if( task == "CMD" )
        {
          QString cmd = msg.getValue<QString>("cmd");
          for(auto socket = _clients.begin(); socket != _clients.end(); ++socket)
          {
            if(    sender() != socket->first
                && socket->second->supportsCommand(cmd) )
              return (void)socket->first->sendTextMessage(data);
          }
          return;
        }
      }

      QString id = msg.getValue<QString>("id").trimmed(),
              title = msg.getValue<QString>("title", "");
      const std::string& id_str = to_string(id);

      if( task == "GET" )
      {
        QString val;
        if( id == "/routing" )
        {
          val = "{\"active\": \""
              + QString::fromStdString(_subscribe_routing->_data->active)
              + "\", \"default\": \""
              + QString::fromStdString(_subscribe_routing->_data->getDefault())
              + "\", \"available\":[";
          if( !_subscribe_routing->_data->available.empty() )
          {
            for( auto comp = _subscribe_routing->_data->available.begin();
                 comp != _subscribe_routing->_data->available.end();
                 ++comp )
              val += "[\"" + QString::fromStdString(comp->first) + "\","
                   + (comp->second ? '1' : '0') + "],";
            val.replace(val.length() - 1, 1, ']');
          }
          else
            val.push_back(']');
          val.push_back('}');
        }
        else if( id == "/search-history" )
        {
          std::string history;
          (*_subscribe_user_config->_data)->getString("QtWebsocketServer:SearchHistory", history);
          val = "\""
              + QString::fromStdString(history).replace('"', "\\\"")
              + "\"";
        }
        else
        {
          std::string val_std;
          if( !(*_subscribe_user_config->_data)
               ->getParameter
                (
                  id_str,
                  val_std,
                  msg.getValue<std::string>("type")
                ) )
          {
            LOG_WARN("Requesting unknown value: " << id_str);
            return;
          }

          val = "\""
              + QString::fromStdString(val_std).replace('"', "\\\"")
              + "\"";
        }

        client->first->sendTextMessage
        ("{"
            "\"task\": \"GET-FOUND\","
            "\"id\": \"" + id.replace('"', "\\\"") + "\","
            "\"val\":" + val +
         "}");

        return;
      }
      else if( task == "SET" )
      {
        if( id == "/routing" )
        {
          _subscribe_routing->_data->request = msg.getValue<std::string>("val");
          for( auto link = _slot_links->_data->begin();
                   link != _slot_links->_data->end();
                   ++link )
            link->_stamp += 1;
          LOG_INFO("Trigger reroute -> routing algorithm changed");
          dirtyLinks();
        }
        else
        {
          if( !(*_subscribe_user_config->_data)
               ->setParameter
                (
                  id_str,
                  msg.getValue<std::string>("val"),
                  msg.getValue<std::string>("type")
                ) )
          {
            LOG_WARN("Request setting unknown value: " << id_str);
            return;
          }
          dirtyLinks();
        }
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
        onInitiate(link, id, stamp, msg, client_info);
      else if( task == "FOUND" || task == "UPDATE" )
      {
        if( link == _slot_links->_data->end() )
        {
          LOG_WARN("Received FOUND for none existing REQUEST");
          return;
        }

//        if( link->_stamp != stamp )
//        {
//          LOG_WARN("Received FOUND for wrong REQUEST (different stamp)");
//          return;
//        }

        LOG_INFO("Received "  << task << ": " << id_str);

        client_info.parseScrollRegion(msg);

        if( task == "FOUND" )
        {
          link->_link->addNode(
            client_info.parseRegions(msg)
          );
        }
        else
        {
          LinkDescription::NodePtr node;
          for(auto& n: link->_link->getNodes())
          {
            if( n->getChildren().empty() )
              continue;

            auto& hedge = n->getChildren().front();
            if(    hedge->get<WId>("client_wid")
                == client_info.getWindowInfo().id )
            {
              node = n;
              break;
            }
          }

          if( !node )
          {
            LOG_WARN("Received UPDATE for none existing client");
            return;
          }

          client_info.parseRegions(msg, node);
        }

        client_info.update(_window_monitor.getWindows());
        updateCenter(link->_link.get());
      }
      else if( task == "ABORT" )
      {
        WId abort_wid = client_info.getWindowInfo().id;

        if( id.isEmpty() && stamp == (uint32_t)-1 )
          abortAll();
        else if( link != _slot_links->_data->end() )
          abortLinking(link, abort_wid);
        else
          LOG_WARN("Received ABORT for none existing REQUEST");

        for(auto socket = _clients.begin(); socket != _clients.end(); ++socket)
          if( sender() != socket->first )
            socket->first->sendTextMessage(data);
      }
      else
      {
        LOG_WARN("Unknown task: " << task);
        return;
      }
    }
    catch(std::runtime_error& ex)
    {
      LOG_WARN("Failed to parse message: " << ex.what());
    }

    dirtyLinks();
  }

  //----------------------------------------------------------------------------
  void IPCServer::onBinaryReceived(QByteArray data)
  {
    if( data.size() < 3 )
    {
      LOG_WARN("Binary message too small (" << data.size() << "byte)");
      return;
    }

    uint8_t type = data.at(0),
            seq_id = data.at(1);

    std::cout << "Binary data received: "
              << ((data.size() / 1024) / 1024.f) << "MB"
              << " type=" << (int)type
              << " seq=" << (int)seq_id << std::endl;

    QMutexLocker lock_links(_mutex_slot_links);

    if( type != 1 )
    {
      LOG_WARN("Invalid binary data!");
      return;
    }

    auto request = _tile_handler->_tile_requests.find(seq_id);
    if( request == _tile_handler->_tile_requests.end() )
    {
      LOG_WARN("Received unknown tile request.");
      return;
    }

    auto req_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>
      (
        clock::now() - request->second.time_stamp
      ).count();

    std::cout << "request #" << (int)seq_id << " " << req_ms << "ms"
              << std::endl;

    HierarchicTileMapPtr tile_map = request->second.tile_map.lock();
    if( !tile_map )
    {
      LOG_WARN("Received tile request for expired map.");
      _tile_handler->_tile_requests.erase(request);
      return;
    }

    tile_map->setTileData( request->second.x,
                           request->second.y,
                           request->second.zoom,
                           data.constData() + 2,
                           data.size() - 2 );

    QImage img( (const uchar*)data.constData() + 2,
                request->second.tile_size.x,
                request->second.tile_size.y,
                QImage::Format_ARGB32 );
    img.save( QString("tile-%1-%2").arg(request->second.x).arg(request->second.y));

    dirtyRender();
  }

  //----------------------------------------------------------------------------
  void IPCServer::onClientDisconnection()
  {
    QWebSocket * socket = qobject_cast<QWebSocket*>(sender());
    if( !socket )
      return;

    _clients.erase(socket);
    socket->deleteLater();

    LOG_INFO("Client disconnected");
  }

  //----------------------------------------------------------------------------
  void IPCServer::onInitiate( LinkDescription::LinkList::iterator& link,
                              const QString& id,
                              uint32_t stamp,
                              const JSONParser& msg,
                              ClientInfo& client_info )
  {
    if( id.length() > 256 )
    {
      LOG_WARN("Search identifier too long!");
      return;
    }

    const std::string id_str = to_string(id);
    LOG_INFO("Received INITIATE: " << id_str);
    uint32_t color_id = 0;
#if 0
    std::string history;
    (*_subscribe_user_config->_data)->getString("QtWebsocketServer:SearchHistory", history);

    QString clean_id = QString(id).replace(",","") + ",";
    QString new_history = clean_id
                        + QString::fromStdString(history).replace(clean_id, "");

    (*_subscribe_user_config->_data)
      ->setString("QtWebsocketServer:SearchHistory", to_string(new_history));
#endif
    client_info.parseScrollRegion(msg);
    client_info.update(_window_monitor.getWindows());

    // Remove eventually existing search for same id
    if( link != _slot_links->_data->end() )
    {
      // keep color
      color_id = link->_color_id;

      LOG_INFO("Replacing search for " << link->_id);
      abortLinking(link);
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
    auto hedge = LinkDescription::HyperEdge::make_shared();
    hedge->set("link-id", id_str);
    hedge->addNode( client_info.parseRegions(msg) );
    client_info.update(_window_monitor.getWindows());
    updateCenter(hedge.get());

    _slot_links->_data->push_back(
      LinkDescription::LinkDescription(id_str, stamp, hedge, color_id)
    );
    _slot_links->setValid(true);

    // Send request to all clients
    QString request(
      "{"
        "\"task\": \"REQUEST\","
        "\"id\": \"" + QString(id).replace('"', "\\\"") + "\","
        "\"stamp\": " + QString::number(stamp) +
      "}"
    );

    for(auto socket = _clients.begin(); socket != _clients.end(); ++socket)
      socket->first->sendTextMessage(request);
  }

  //----------------------------------------------------------------------------
  void IPCServer::regionsChanged(const WindowRegions& regions)
  {
    _window_monitor.setDesktopRect( desktopRect().toQRect() );
    _mutex_slot_links->lock();

    bool need_update = true; // TODO update checks to also detect eg. changes in
                             //      outside icons.
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
    dirtyLinks();
  }

  //----------------------------------------------------------------------------
  class CoverWindows
  {
    public:
      CoverWindows( const WindowInfos::const_iterator& begin,
                    const WindowInfos::const_iterator& end ):
        _begin(begin),
        _end(end)
      {}

      bool valid() const
      {
        return _begin != _end;
      }

      bool hit(const QPoint& point)
      {
        for(auto it = _begin; it != _end; ++it)
          if( it->region.contains(point) )
            return true;
        return false;
      }

      QRect getCoverRegion(const float2& point)
      {
        QPoint p(point.x, point.y);
        for(auto it = _begin; it != _end; ++it)
          if( it->region.contains(p) )
            return it->region;
        return QRect();
      }

      LinkDescription::NodePtr getNearestVisible( const float2& point,
                                                  bool triangle = false )
      {
        const QRect reg = getCoverRegion(point);
        float dist[4] = {
          point.x - reg.left(),
          reg.right() - point.x,
          point.y - reg.top(),
          reg.bottom() - point.y
        };

        float min_dist = dist[0];
        size_t min_index = 0;

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

        return buildIcon(new_center, normal, triangle);
      }

      /**
       *
       * @param p_out       Outside point
       * @param p_cov       Covered point
       * @param triangle
       * @return
       */
      LinkDescription::NodePtr getVisibleIntersect( const float2& p_out,
                                                    const float2& p_cov,
                                                    bool triangle = false )
      {
        const QRect& reg = getCoverRegion(p_cov);
        float2 dir = (p_out - p_cov).normalize();

        float2 normal;
        float fac = -1;

        if( std::fabs(dir.x) > 0.01 )
        {
          // if not too close to vertical try intersecting left or right
          if( dir.x > 0 )
          {
            fac = (reg.right() - p_cov.x) / dir.x;
            normal.x = 1;
          }
          else
          {
            fac = (reg.left() - p_cov.x) / dir.x;
            normal.x = -1;
          }

          // check if vertically inside the region
          float y = p_cov.y + fac * dir.y;
          if( y < reg.top() || y > reg.bottom() )
            fac = -1;
        }

        if( fac < 0 )
        {
          // nearly vertical or horizontal hit outside of vertical region
          // boundaries -> intersect with top or bottom
          normal.x = 0;

          if( dir.y < 0 )
          {
            fac = (reg.top() - p_cov.y) / dir.y;
            normal.y = -1;
          }
          else
          {
            fac = (reg.bottom() - p_cov.y) / dir.y;
            normal.y = 1;
          }
        }

        float2 new_center = p_cov + fac * dir;
        return buildIcon(new_center, normal, triangle);
      }

    protected:
      const WindowInfos::const_iterator& _begin,
                                         _end;

      LinkDescription::NodePtr buildIcon( float2 center,
                                          const float2& normal,
                                          bool triangle ) const
      {
        LinkDescription::points_t link_points, link_points_children;
        link_points_children.push_back(center);
        link_points.push_back(center += 12 * normal);

        LinkDescription::points_t points;

        if( triangle )
        {
          points.push_back(center += 6 * normal.normal());
          points.push_back(center -= 6 * normal.normal() + 12 * normal);
          points.push_back(center -= 6 * normal.normal() - 12 * normal);
        }
        else
        {
          points.push_back(center += 0.5 * normal.normal());
          points.push_back(center -= 9 * normal);

          points.push_back(center += 2.5 * normal.normal());
          points.push_back(center += 6   * normal.normal() + 3 * normal);
          points.push_back(center += 1.5 * normal.normal() - 3 * normal);
          points.push_back(center -= 6   * normal.normal() + 3 * normal);
          points.push_back(center -= 9   * normal.normal());
          points.push_back(center -= 6   * normal.normal() - 3 * normal);
          points.push_back(center += 1.5 * normal.normal() + 3 * normal);
          points.push_back(center += 6   * normal.normal() - 3 * normal);
          points.push_back(center += 2.5 * normal.normal());

          points.push_back(center += 9 * normal);
        }

        auto node = std::make_shared<LinkDescription::Node>(
          points,
          link_points,
          link_points_children
        );
        node->set("virtual-covered", true);
        //node->set("filled", true);

        return node;
      }
  };

  //----------------------------------------------------------------------------
  IPCServer::ClientInfos::iterator IPCServer::findClientInfo(WId wid)
  {
    return std::find_if
    (
      _clients.begin(),
      _clients.end(),
      [wid](const ClientInfos::value_type& it)
      {
        return it.second->getWindowInfo().id == wid;
      }
    );
  }

  //----------------------------------------------------------------------------
  QWebSocket* IPCServer::getSocketByWId(WId wid)
  {
    auto client = findClientInfo(wid);
    (
      _clients.begin(),
      _clients.end(),
      [wid](const ClientInfos::value_type& c)
      {
        return c.second->getWindowInfo().id == wid;
      }
    );

    if( client == _clients.end() )
      return 0;

    return client->first;
  }

  //----------------------------------------------------------------------------
  bool IPCServer::updateHedge( const WindowRegions& regions,
                               LinkDescription::HyperEdge* hedge )
  {
    bool modified = false;
    hedge->resetNodeParents();
    WId client_wid = hedge->get<WId>("client_wid", 0);

    QRect region, scroll_region;
    auto client_info = findClientInfo(client_wid);
    if( client_info != _clients.end() )
      modified |= client_info->second->update(regions);

    const LinkDescription::nodes_t nodes = hedge->getNodes();
    for( auto node = hedge->getNodes().begin();
              node != hedge->getNodes().end();
            ++node )
    {
      for(auto child = (*node)->getChildren().begin();
               child != (*node)->getChildren().end();
             ++child )
        modified |= updateHedge(regions, child->get());
    }

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
//    std::cout << " - center = " << hedge_center << hedge_center/num_visible << std::endl;

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
      if( client_wid != regions.windowIdAt(QPoint(vert->x, vert->y)) )
        ++hidden_count;
    }

    LinkDescription::PropertyMap& props = node->getProps();
    bool was_hidden = props.get<bool>("hidden"),
         hidden = hidden_count > node->getVertices().size() / 2;

    if( hidden != was_hidden )
    {
      props.set("hidden", hidden);
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  void IPCServer::onClick(int x, int y)
  {
    QMutexLocker lock_links(_mutex_slot_links);

    bool changed = foreachPopup([&]( SlotType::TextPopup::Popup& popup,
                                     QWebSocket& socket,
                                     ClientInfo& client_info ) -> bool
    {
      if( popup.region.contains(x, y) )
      {
        client_info.activateWindow();
        popup.hover_region.fadeIn();
        _tile_handler->updateRegion(popup);
        return true;
      }

      if(    !popup.hover_region.isVisible()
          || !popup.hover_region.contains(x, y) )
        return false;

      popup.hover_region.fadeOut();

      QSize preview_size = client_info.preview_size;
      float scale = (preview_size.height()
                    / pow(2.0f, static_cast<float>(popup.hover_region.zoom))
                    )
                  / _preview_height;
      float dy = y - popup.hover_region.region.pos.y,
            scroll_y = scale * dy + popup.hover_region.src_region.pos.y;

      for( auto part_src = client_info.tile_map->partitions_src.begin(),
                part_dst = client_info.tile_map->partitions_dest.begin();
                part_src != client_info.tile_map->partitions_src.end()
             && part_dst != client_info.tile_map->partitions_dest.end();
              ++part_src,
              ++part_dst )
      {
        if( scroll_y <= part_dst->y )
        {
          scroll_y = part_src->x + (scroll_y - part_dst->x);
          break;
        }
      }

      socket.sendTextMessage(QString(
      "{"
        "\"task\": \"SET\","
        "\"id\": \"scroll-y\","
        "\"val\": " + QString("%1").arg(scroll_y - 35) +
      "}"));
      client_info.activateWindow();
      return true;
    });

    changed |= foreachPreview([&]( SlotType::XRayPopup::HoverRect& preview,
                                   QWebSocket& socket,
                                   ClientInfo& client_info ) -> bool
    {
      float2 offset = preview.node->getParent()->get<float2>("screen-offset");
      if(    !preview.region.contains(float2(x, y) - offset)
          && !(preview.isVisible() && preview.preview_region.contains(x, y)) )
        return false;

      preview.node->getParent()->setOrClear("hover", false);
      preview.node->setOrClear("hover", false);
      preview.fadeOut();
      client_info.activateWindow();

      if( !preview.node->get<bool>("outside") )
      {
        // We need to scroll only for regions outside the viewport
        return true;
      }

      QRect client_region = client_info.getViewportAbs();

      const bool scroll_center = true;
      int scroll_y = y;

      if( scroll_center )
      {
        scroll_y -= (client_region.top() + client_region.bottom()) / 2;
      }
      else
      {
        scroll_y -= client_region.top();
        if( scroll_y > 0 )
          // if below scroll up until region is visible
          scroll_y = y - client_region.bottom() + 35;
        else
          // if above scroll down until region is visible
          scroll_y -= 35;
      }

      scroll_y -= client_info.scroll_region.top();

      socket.sendTextMessage(QString(
        "{"
          "\"task\": \"SET\","
          "\"id\": \"scroll-y\","
          "\"val\": " + QString("%1").arg(scroll_y) +
        "}"));

      return true;
    });

    if( changed )
      return dirtyRender();
  }

  void print( const LinkDescription::Node& node,
              const std::string& key,
              const std::string& indent = "" );

  void print( const LinkDescription::HyperEdge& hedge,
              const std::string& key,
              const std::string& indent = "" )
  {
    std::cout << indent << "HyperEdge(" << &hedge << ") "
              << key << " = '" << hedge.getProps().get<std::string>(key) << "'"
              << std::endl;

    for(auto const& c: hedge.getNodes())
      print(*c, key, indent + " ");
  }

  void print( const LinkDescription::Node& node,
              const std::string& key,
              const std::string& indent )
  {
    std::cout << indent << "Node(" << &node << ") "
              << key << " = '" << node.getProps().get<std::string>(key) << "'"
              << std::endl;

    for(auto const& c: node.getChildren())
      print(*c, key, indent + " ");
  }

  //----------------------------------------------------------------------------
  void IPCServer::onMouseMove(int x, int y)
  {
    QMutexLocker lock_links(_mutex_slot_links);

    bool popup_visible = false;
    SlotType::TextPopup::Popup* hide_popup = nullptr;
    bool changed = foreachPopup([&]( SlotType::TextPopup::Popup& popup,
                                     QWebSocket& socket,
                                     ClientInfo& client_info ) -> bool
    {
      auto& reg = popup.hover_region;
      if(    !popup_visible // Only one popup at the same time
          && (  popup.region.contains(x, y)
             || (reg.isVisible() && reg.contains(x,y))
             ) )
      {
//        if( hide_popup )
//          hide_popup->hover_region.fadeOut();

        popup_visible = true;
        if( reg.isVisible() && !reg.isFadeOut() )
          return false;

        if( !reg.isFadeOut() )
          reg.delayedFadeIn();

        _tile_handler->updateRegion(popup);
        return true;
      }
      else if( reg.isVisible() )
      {
        if( !popup.hover_region.isFadeOut() )
          popup.hover_region.delayedFadeOut();
        else
          // timeout already started, so store to be able hiding if other
          // popup is shown before hiding this one.
          hide_popup = &popup;
      }
      else if( reg.isFadeIn() )
      {
        reg.hide();
      }

      return false;
    });

    bool preview_visible = false;
    changed |= foreachPreview([&]( SlotType::XRayPopup::HoverRect& preview,
                                   QWebSocket& socket,
                                   ClientInfo& client_info ) -> bool
    {
      auto const& p = preview.node->getParent();

      if(   !preview_visible // Only one preview at the same time
          && ( preview.region.contains( float2(x, y)
                                      - p->get<float2>("screen-offset"))
            || (preview.isVisible() && preview.preview_region.contains(x,y))
             )
        )
      {
        preview_visible = true;
        if( preview.isVisible() && !preview.isFadeOut() )
          return false;

        if( preview.isFadeOut() )
          preview.fadeIn();
        else
          preview.delayedFadeIn();

        _tile_handler->updateRegion(preview);
        return true;
      }
      else if( preview.isVisible() )
      {
        if( !preview.isFadeOut() && !preview_visible )
          preview.delayedFadeOut();
      }
      else if( preview.isFadeIn() )
      {
        preview.hide();
      }

      return false;
    });

    if( changed )
      dirtyRender();
  }

  //----------------------------------------------------------------------------
  void IPCServer::onScrollWheel(int delta, const float2& pos, uint32_t mod)
  {
    QMutexLocker lock_links(_mutex_slot_links);

    float preview_aspect = _preview_width
                         / static_cast<float>(_preview_height);

    if( foreachPopup([&]( SlotType::TextPopup::Popup& popup,
                      QWebSocket& socket,
                      ClientInfo& client_info ) -> bool
    {
      if( !popup.hover_region.isVisible() )
        return false;

      bool changed = false;
      const float2& scroll_size = popup.hover_region.scroll_region.size;
      float step_y = scroll_size.y
                   / pow(2.0f, static_cast<float>(popup.hover_region.zoom))
                   / _preview_height;

      if( mod & SlotType::MouseEvent::ShiftModifier )
      {
        popup.hover_region.src_region.pos.y -=
          delta / fabs(static_cast<float>(delta)) * 20 * step_y;
        changed |= _tile_handler->updateRegion(popup);
      }
      else if( mod & SlotType::MouseEvent::ControlModifier )
      {
        float step_x = step_y * preview_aspect;
        popup.hover_region.src_region.pos.x -=
          delta / fabs(static_cast<float>(delta)) * 20 * step_x;
        changed |= _tile_handler->updateRegion(popup);
      }
      else
      {
        //pow(2.0f, static_cast<float>(popup.hover_region.zoom)) = scroll_size.y / _preview_height;
        int max_zoom = log2(scroll_size.y / _preview_height / 0.9) + 0.7;
        int old_zoom = popup.hover_region.zoom;
        clampedStep(popup.hover_region.zoom, delta, 0, max_zoom, 1);

        if( popup.hover_region.zoom != old_zoom )
        {
          float scale = (scroll_size.y / pow(2.0f, static_cast<float>(old_zoom)))
                      / _preview_height;
          Rect const old_region = popup.hover_region.region;

          float2 d = pos - popup.hover_region.region.pos,
                 mouse_pos = scale * d + popup.hover_region.src_region.pos;
          _tile_handler->updateRegion
          (
            popup,
            mouse_pos,
            d / popup.hover_region.region.size
          );

          if( old_region != popup.hover_region.region )
            _dirty_flags |= MASK_DIRTY;

          changed = true;
        }
      }

      return changed;
    }) )
      dirtyRender();
  }

  //----------------------------------------------------------------------------
  void IPCServer::onDrag(const float2& delta)
  {
    QMutexLocker lock_links(_mutex_slot_links);

    float preview_aspect = _preview_width
                         / static_cast<float>(_preview_height);

    if( foreachPopup([&]( SlotType::TextPopup::Popup& popup,
                          QWebSocket& socket,
                          ClientInfo& client_info ) -> bool
    {
      if( !popup.hover_region.isVisible() )
        return false;

      const float2& scroll_size = popup.hover_region.scroll_region.size;
      float step_y = scroll_size.y
                   / pow(2.0f, static_cast<float>(popup.hover_region.zoom))
                   / _preview_height;

      popup.hover_region.src_region.pos.y -= delta.y * step_y;
      float step_x = step_y * preview_aspect;
      popup.hover_region.src_region.pos.x -= delta.x * step_x;

      return _tile_handler->updateRegion(popup);
    }) )
      dirtyRender();
  }

  //----------------------------------------------------------------------------
  bool IPCServer::foreachPopup(const IPCServer::popup_callback_t& cb)
  {
    bool changed = false;
    for( auto popup = _subscribe_popups->_data->popups.begin();
              popup != _subscribe_popups->_data->popups.end(); )
    {
      if( !popup->client_socket )
      {
        ++popup;
        continue;
      }

      QWebSocket* client_socket = static_cast<QWebSocket*>(popup->client_socket);
      ClientInfos::iterator client = _clients.find(client_socket);

      if( client == _clients.end() )
      {
        LOG_WARN("Popup without valid client_socket");
        popup->client_socket = 0;
        // deleting would result in invalid iterators inside ClientInfo, so
        // just let it be and wait for the according ClientInfo to remove it.
        //popup = _subscribe_popups->_data->popups.erase(popup);
        ++popup;
        changed = true;
        continue;
      }

      changed |= cb(*popup, *client_socket, *client->second);
      ++popup;
    }

    return changed;
  }


  //----------------------------------------------------------------------------
  bool IPCServer::foreachPreview(const IPCServer::preview_callback_t& cb)
  {
    bool changed = false;
    for( auto preview = _slot_xray->_data->popups.begin();
              preview != _slot_xray->_data->popups.end(); )
    {
      if( !preview->client_socket )
      {
        ++preview;
        continue;
      }

      auto const& p = preview->node->getParent();
      if( !p )
      {
        LOG_WARN("xray preview: parent lost");

        // Hide preview if parent has somehow been lost
        changed |= preview->node->setOrClear("hover", false);

        // deleting would result in invalid iterators inside ClientInfo, so
        // just let it be and wait for the according ClientInfo to remove it.
        //preview = _slot_xray->_data->popups.erase(preview);
        ++preview;
        continue;
      }

      QWebSocket* client_socket = static_cast<QWebSocket*>(preview->client_socket);
      ClientInfos::iterator client = _clients.find(client_socket);

      if( client == _clients.end() )
      {
        LOG_WARN("Preview without valid client_socket");
        preview->client_socket = 0;

        // Hide preview if socket has somehow been lost
        changed |= preview->node->setOrClear("hover", false);

        // deleting would result in invalid iterators inside ClientInfo, so
        // just let it be and wait for the according ClientInfo to remove it.
        //preview = _slot_xray->_data->popups.erase(preview);
        ++preview;
        continue;
      }

      changed |= cb(*preview, *client_socket, *client->second);
      ++preview;
    }

    return changed;
  }

  //----------------------------------------------------------------------------
  void IPCServer::dirtyLinks()
  {
    _dirty_flags |= LINKS_DIRTY | RENDER_DIRTY | MASK_DIRTY;
    _cond_data_ready->wakeAll();
  }

  //----------------------------------------------------------------------------
  void IPCServer::dirtyRender()
  {
    _dirty_flags |= RENDER_DIRTY;
    _cond_data_ready->wakeAll();
  }

  //----------------------------------------------------------------------------
  LinkDescription::LinkList::iterator
  IPCServer::abortLinking( const LinkDescription::LinkList::iterator& link,
                           WId wid )
  {
    int remove_count = 0;
    for(auto& client: _clients)
    {
      if( !wid || client.second->getWindowInfo().id == wid )
      {
        qDebug() << "remove link for " << client.second->getWindowInfo().title;
        client.second->removeLink(link->_link.get());
        remove_count += 1;
      }
    }

    return remove_count == _clients.size() ? _slot_links->_data->erase(link)
                                           : link;
  }

  //----------------------------------------------------------------------------
  void IPCServer::abortAll()
  {
    for(auto link = _slot_links->_data->begin();
             link != _slot_links->_data->end(); )
      link = abortLinking(link);
    assert( _slot_links->_data->empty() );
  }

  //----------------------------------------------------------------------------
  IPCServer::TileHandler::TileHandler(IPCServer* ipc_server):
    _ipc_server(ipc_server),
    _tile_request_id(0),
    _new_request(0)
  {

  }

  //----------------------------------------------------------------------------
  bool IPCServer::TileHandler::updateRegion( SlotType::TextPopup::Popup& popup,
                                             float2 center,
                                             float2 rel_pos )
  {
    HierarchicTileMapPtr tile_map = popup.hover_region.tile_map.lock();
    float scale = 1/tile_map->getLayerScale(popup.hover_region.zoom);

    float preview_aspect = _ipc_server->_preview_width
                         / static_cast<float>(_ipc_server->_preview_height);
    const QRect reg = popup.hover_region.scroll_region.toQRect();
    float zoom_scale = pow(2.0f, static_cast<float>(popup.hover_region.zoom));

    int h = reg.height() / zoom_scale + 0.5f,
        w = h * preview_aspect + 0.5;

    Rect& src = popup.hover_region.src_region;
    //float2 old_pos = src.pos;

    src.size.x = w;
    src.size.y = h;

    if( popup.auto_resize )
    {
      int real_width = reg.width() / scale + 0.5f,
          popup_width = std::min(_ipc_server->_preview_width, real_width);

      Rect& popup_region = popup.hover_region.region;
      const Rect& icon_region = popup.region.region;
      float offset_l = popup_region.pos.x - icon_region.pos.x,
            offset_r = icon_region.size.x - popup_region.size.x - offset_l;

      if( std::signbit(offset_l) == std::signbit(offset_r) )
      {
        int center_x = popup_region.pos.x + 0.5 * popup_region.size.x;
        popup_region.size.x = popup_width;
        popup_region.pos.x = center_x - popup_width / 2;
      }
      else
      {
        if( icon_region.pos.x > popup_region.pos.x )
          popup_region.pos.x += popup_region.size.x - popup_width;
        popup_region.size.x = popup_width;
      }
    }

    if( center.x > -9999 && center.y > -9999 )
    {
      // Center source region around mouse cursor
      src.pos.x = center.x - rel_pos.x * w;
      src.pos.y = center.y - rel_pos.y * h;
    }

    clamp<float>(src.pos.x, 0, reg.width() - w);
    clamp<float>(src.pos.y, 0, reg.height() - h);

//    if( old_pos == src.pos )
//      return;

    if( !popup.client_socket )
    {
      LOG_WARN("Popup without active socket.");
      return false;
    }

    QWebSocket* socket = static_cast<QWebSocket*>(popup.client_socket);
    ClientInfos::iterator client = _ipc_server->_clients.find(socket);

    if( client == _ipc_server->_clients.end() )
    {
      LOG_WARN("Popup without valid client_socket");
      popup.client_socket = 0;

      return true;
    }

    return !updateTileMap(tile_map, socket, src, popup.hover_region.zoom);
  }

  //----------------------------------------------------------------------------
  bool IPCServer::TileHandler::updateRegion(
    SlotType::XRayPopup::HoverRect& popup
  )
  {
    return updateTileMap( popup.tile_map.lock(),
                          static_cast<QWebSocket*>(popup.client_socket),
                          popup.source_region,
                          -1 );
  }

  //----------------------------------------------------------------------------
  bool IPCServer::TileHandler::updateTileMap(
    const HierarchicTileMapPtr& tile_map,
    QWebSocket* socket,
    const Rect& src,
    int zoom )
  {
    if( !tile_map )
    {
      LOG_WARN("Tilemap has expired!");
      return false;
    }

    bool sent = false;
    MapRect rect = tile_map->requestRect(src, zoom);
    rect.foreachTile([&](Tile& tile, size_t x, size_t y)
    {
      if( tile.type == Tile::NONE )
      {
        auto req = std::find_if
        (
          _tile_requests.begin(),
          _tile_requests.end(),
          //TODO VS FIX
          [&](const std::map<uint8_t, TileRequest>::value_type& tile_req)
          //[&](const TileRequests::value_type& tile_req)
          {
            return (tile_req.second.tile_map.lock() == tile_map)
                && (tile_req.second.zoom == zoom)
                && (tile_req.second.x == x)
                && (tile_req.second.y == y);
          }
        );

        if( req != _tile_requests.end() )
          // already sent
          return;

        TileRequest tile_req = {
          socket,
          tile_map,
          zoom,
          x, y,
          float2(tile.width, tile.height),
          clock::now()
        };
        uint8_t req_id = ++_tile_request_id;
        _tile_requests[req_id] = tile_req;
        sent = true;
      }
    });
    if( sent )
      _new_request = 2;
    return sent;
  }

} // namespace LinksRouting
