/*
 * ClientInfo.cxx
 *
 *  Created on: Apr 8, 2013
 *      Author: tom
 */

#include <QPoint>
#include <QRect>
#include "ClientInfo.hxx"
#include "ipc_server.hpp"

#include <cassert>

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  ClientInfo::ClientInfo(IPCServer* ipc_server, WId wid):
    _dirty(~0),
    _ipc_server(ipc_server),
    _window_info(wid),
    _minimized_icon(std::make_shared<LinkDescription::Node>()),
    _avg_region_height(0)
  {
    _minimized_icon->set("filled", true);
  }

  //----------------------------------------------------------------------------
  void ClientInfo::setWindowId(WId wid)
  {
    _window_info.id = wid;
  }

  //----------------------------------------------------------------------------
  const WindowInfo& ClientInfo::getWindowInfo() const
  {
    return _window_info;
  }

  //----------------------------------------------------------------------------
  void ClientInfo::parseScrollRegion( const JSONParser& json )
  {
    if( json.hasChild("scroll-region") )
      scroll_region = json.getValue<QRect>("scroll-region");
    else
      // If no scroll-region is given assume only content within viewport is
      // accessible
      scroll_region = QRect(QPoint(0,0), viewport.size());

    _dirty |= SCROLL_POS | SCROLL_SIZE;
  }

  //----------------------------------------------------------------------------
  void ClientInfo::setScrollPos( const QPoint& offset )
  {
    scroll_region.setTopLeft(offset);
    _dirty |= SCROLL_POS;

    for(auto& popup: _popups)
      popup->hover_region.offset = -offset;
  }

  //----------------------------------------------------------------------------
  LinkDescription::NodePtr ClientInfo::parseRegions
  (
    const JSONParser& json,
    LinkDescription::NodePtr node
  )
  {
    std::cout << "Parse regions (" << _window_info.id<< ")" << std::endl;
    _avg_region_height = 0;

    _dirty |= REGIONS;

    if( !json.hasChild("regions") )
      return std::make_shared<LinkDescription::Node>();

    float2 top_left = getViewportAbs().topLeft(),
           scroll_offset = scroll_region.topLeft();

    LinkDescription::nodes_t nodes;

    QVariantList regions = json.getValue<QVariantList>("regions");
    for(auto region = regions.begin(); region != regions.end(); ++region)
    {
      LinkDescription::points_t points;
      LinkDescription::PropertyMap props;

      float min_y = std::numeric_limits<float>::max(),
            max_y = std::numeric_limits<float>::lowest();

      //for(QVariant point: region.toList())
      auto regionlist = region->toList();
      for(auto point = regionlist.begin(); point != regionlist.end(); ++point)
      {
        if( point->type() == QVariant::Map )
        {
          auto map = point->toMap();
          for( auto it = map.constBegin(); it != map.constEnd(); ++it )
            props.set(to_string(it.key()), it->toString());
        }
        else
        {
          points.push_back( JSONNode(*point).getValue<float2>() );

          if( points.back().y > max_y )
            max_y = points.back().y;
          else if( points.back().y < min_y )
            min_y = points.back().y;
        }
      }

      if( points.empty() )
        continue;

      _avg_region_height += max_y - min_y;
      float2 center;

      bool rel = props.get<bool>("rel");
      for(size_t i = 0; i < points.size(); ++i)
      {
        // Transform to local coordinates within applications scrollable area
        if( !rel )
          points[i] -= top_left;
        points[i] -= scroll_offset;
        center += points[i];
      }

      center /= points.size();
      LinkDescription::points_t link_points;
      link_points.push_back(center);

      nodes.push_back
      (
        std::make_shared<LinkDescription::Node>(points, link_points, props)
      );
    }

    if( !nodes.empty() )
      _avg_region_height /= nodes.size();

    auto hedge = std::make_shared<LinkDescription::HyperEdge>(nodes);
    hedge->addNode(_minimized_icon);
    //updateHedge(_window_monitor.getWindows(), hedge.get());

    if( !node )
    {
      node = std::make_shared<LinkDescription::Node>(hedge);
      std::cout << "add node " << node.get() << std::endl;
      _nodes.push_back(node);
    }
    else
    {
      node->clearChildren();
      node->addChild(hedge);
    }
    return node;
  }

  //----------------------------------------------------------------------------
  bool ClientInfo::update(const WindowRegions& windows)
  {
    auto window_info = windows.find(_window_info.id);
    if( window_info == windows.end() )
      return false;

    if( _window_info != *window_info )
    {
      _window_info = *window_info;
      _dirty |= WINDOW;
    }

    updateRegions(windows);

    if( !_dirty )
      return false;

    if( _dirty & (SCROLL_SIZE | REGIONS) )
      updateTileMap();

    for(auto& node: _nodes)
      updateHedges( node->getChildren() );

    _dirty = 0;
    return true;
  }

  //----------------------------------------------------------------------------
  void ClientInfo::removeLink(LinkDescription::HyperEdge* hedge)
  {
    for( auto node = _nodes.begin(); node != _nodes.end(); )
    {
      if( (*node)->getParent() == hedge )
        node = _nodes.erase(node);
      else
        ++node;
    }

    const std::string id = hedge->get<std::string>("link-id");
    for( auto popup = _popups.begin(); popup != _popups.end(); )
    {
      if( (*popup)->link_id == id )
      {
        _ipc_server->removePopup( *popup );
        popup = _popups.erase(popup);
      }
      else
        ++popup;
    }
  }

  //----------------------------------------------------------------------------
  QRect ClientInfo::getViewportAbs() const
  {
    return viewport.translated( _window_info.region.topLeft() );
  }

  //----------------------------------------------------------------------------
  QRect ClientInfo::getScrollRegionAbs() const
  {
    return scroll_region.translated( getViewportAbs().topLeft() );
  }

  //----------------------------------------------------------------------------
  void ClientInfo::activateWindow()
  {
    if( _window_info.isValid() )
      QxtWindowSystem::activeWindow( _window_info.id );
  }

  //----------------------------------------------------------------------------
  float2 ClientInfo::getPreviewSize() const
  {
    return float2( _ipc_server->getPreviewWidth(),
                   _ipc_server->getPreviewHeight() );
  }

  //----------------------------------------------------------------------------
  void ClientInfo::createPopup( const float2& pos,
                                const float2& normal,
                                const std::string& text,
                                const LinkDescription::nodes_t& nodes,
                                const std::string& link_id,
                                bool auto_resize )
  {
    const QRect& desktop_rect = _ipc_server->desktopRect();

    float2 popup_pos, popup_size(text.length() * 10 + 6, 16);
    float2 hover_pos, hover_size = getPreviewSize();

    const size_t border_text = 4,
                 border_preview = 8;

    if( std::fabs(normal.y) > 0.5 )
    {
      popup_pos.x = pos.x - popup_size.x / 2;
      hover_pos.x = pos.x - hover_size.x / 2;

      if( normal.y < 0 )
      {
        popup_pos.y = pos.y - 13 - popup_size.y - border_text;
        hover_pos.y = popup_pos.y - hover_size.y + border_text
                                                 - 2 * border_preview;
      }
      else
      {
        popup_pos.y = pos.y + 13 + border_text;
        hover_pos.y = popup_pos.y + popup_size.y - border_text
                                                 + 2 * border_preview;
      }
    }
    else
    {
      popup_pos.x = pos.x + normal.x * (13 + border_text);
      hover_pos.x = popup_pos.x + normal.x * (border_preview + border_text);
      if( normal.x > 0 )
        hover_pos.x += popup_size.x;
      else
      {
        popup_pos.x -= popup_size.x;
        hover_pos.x -= hover_size.x + popup_size.x;
      }

      popup_pos.y = pos.y - popup_size.y / 2;
      hover_pos.y = pos.y - hover_size.y / 2;

      clamp<float>( hover_pos.y,
                    desktop_rect.top() + border_preview,
                    desktop_rect.bottom() - border_preview - hover_size.y );
    }

    using SlotType::TextPopup;
    TextPopup::Popup popup = {
      text,
      link_id,
      nodes,
      0,
      TextPopup::HoverRect(popup_pos, popup_size, border_text, true),
      TextPopup::HoverRect(hover_pos, hover_size, border_preview, false),
      auto_resize && _ipc_server->getPreviewAutoWidth()
    };
    popup.hover_region.offset = -scroll_region.topLeft();
    popup.hover_region.dim = viewport.size();
    popup.hover_region.scroll_region.size = preview_size;
    popup.hover_region.tile_map = tile_map;
    _popups.push_back( _ipc_server->addPopup(*this, popup) );
  }

  //----------------------------------------------------------------------------
  void ClientInfo::updateRegions(const WindowRegions& windows)
  {
    _ipc_server->removePopups(_popups);
    _popups.clear();

    if( true ) //_dirty & WINDOW )
    {
      LinkDescription::points_t& icon = _minimized_icon->getVertices();
      icon.clear();

      if( _window_info.minimized )
      {
        const QRect& region_launcher = _window_info.region_launcher;
        QPoint pos
        (
          region_launcher.right() + 11,
          (region_launcher.top() + region_launcher.bottom()) / 2
        );

        const int ICON_SIZE = 10;

        icon.push_back( pos );
        icon.push_back( pos + QPoint(ICON_SIZE, ICON_SIZE) );
        icon.push_back( pos + QPoint(ICON_SIZE,-ICON_SIZE) );

        createPopup
        (
          pos,
          float2(1,0),
          "5",
          _nodes.front()->getChildren().front()->getNodes(),
          _nodes.front()->getParent()->get<std::string>("link-id"),
          false
        );
      }
    }

    auto first_above = windows.find(_window_info.id);
    if( first_above != windows.end() )
      first_above = first_above + 1;

    bool modified = false;
    QRect desktop = _ipc_server->desktopRect()
                                .translated( -getScrollRegionAbs().topLeft() ),
          local_view(-scroll_region.topLeft(), viewport.size());

    // Limit outside indicators to desktop region
    local_view = local_view.intersect(desktop);

    for(auto& node: _nodes)
      for(auto& hedge: node->getChildren())
      {
        LinkDescription::nodes_t changed_nodes;

        /*
         * Check for regions scrolled away and not visualized by any see-
         * through visualization.
         */
        OutsideScroll outside_scroll[4] =
        {
          {float2(0, local_view.top()),    float2( 0, 1), 0},
          {float2(0, local_view.bottom()), float2( 0,-1), 0},
          {float2(local_view.left(),  0),  float2( 1, 0), 0},
          {float2(local_view.right(), 0),  float2(-1, 0), 0}
        };

        for( auto region = hedge->getNodes().begin();
                  region != hedge->getNodes().end(); )
        {
          if( *region == _minimized_icon )
          {
            ++region;
            continue;
          }

          modified |= (*region)->set("hidden", _window_info.minimized);

          if( _window_info.minimized )
          {
            ++region;
            continue;
          }

          if( !(*region)->get<std::string>("outside-scroll").empty() )
          {
            region = hedge->getNodes().erase(region);
            continue;
          }

          LinkDescription::hedges_t& children = (*region)->getChildren();
          for( auto child = children.begin(); child != children.end(); )
          {
            if( modified |= updateChildren( **child,
                                            desktop, local_view,
                                            windows, first_above ) )
            {
              if( (*child)->get<bool>("outside") )
              {
                assert( (*child)->getNodes().size() == 1 );
                LinkDescription::NodePtr outside_node =
                  (*child)->getNodes().front();

                if( !outside_node->get<bool>("outside") )
                {
                  // Remove HyperEdge and move node back after it has moved back
                  // into the viewport.
                  changed_nodes.push_back( outside_node );
                  child = children.erase(child);
                  continue;
                }
              }
            }

            ++child;
          }

          if( modified |= updateNode( **region,
                                      desktop, local_view,
                                      windows, first_above ) )
          {
            if(   !(*region)->getVertices().empty()
                && (*region)->get<bool>("hidden")
                && (*region)->get<bool>("outside") )
            {
              float2 center = (*region)->getCenter();
              for(auto& out: outside_scroll)
              {
                if( (center - out.pos) * out.normal > 0 )
                  continue;

                if( out.normal.x == 0 )
                  out.pos.x += center.x;
                else
                  out.pos.y += center.y;
                out.num_outside += 1;

                break;
              }
            }

            if(    (*region)->get<bool>("on-screen")
                && (*region)->get<bool>("outside") )
            {
#if 0
              // Insert HyperEdge for outside node to allow for special route
              // with "tunnel" icon
              LinkDescription::NodePtr outside_node = *region;
              region = hedge->removeNode(region);

              auto new_hedge = std::make_shared<LinkDescription::HyperEdge>();
              new_hedge->set("no-parent-route", true);
              new_hedge->set("outside", true);
              new_hedge->addNode(outside_node);

              auto new_node = std::make_shared<LinkDescription::Node>();
              new_node->addChild(new_hedge);
              hedge->addNode(new_node);
#endif
            }
          }

          ++region;
        }

        hedge->addNodes(changed_nodes);

        for( size_t i = 0; i < 4; ++i )
        {
          OutsideScroll& out = outside_scroll[i];
          if( !out.num_outside )
            continue;

          if( out.normal.x == 0 )
          {
            out.pos.x /= out.num_outside;
            clamp<float>
            (
              out.pos.x,
              local_view.left() + 12,
              local_view.right() - 12
            );
          }
          else
          {
            out.pos.y /= out.num_outside;
            clamp<float>
            (
              out.pos.y,
              local_view.top() + 12,
              local_view.bottom() - 12
            );
          }

          float2 pos = out.pos;
          LinkDescription::points_t link_points;
          link_points.push_back(pos += 7 * out.normal);

          LinkDescription::points_t points;
          points.push_back(pos +=  3 * out.normal + 12 * out.normal.normal());
          points.push_back(pos -= 10 * out.normal + 12 * out.normal.normal());
          points.push_back(pos += 10 * out.normal - 12 * out.normal.normal());

          auto new_node = std::make_shared<LinkDescription::Node>(points, link_points);
          new_node->set("outside-scroll", "side[" + std::to_string(static_cast<unsigned long long>(i)) + "]");
          new_node->set("filled", true);

          updateNode(*new_node, desktop, local_view, windows, first_above);
          hedge->addNode(new_node);

          createPopup( out.pos + getScrollRegionAbs().topLeft(),
                       out.normal,
                       std::to_string(static_cast<unsigned long long>(out.num_outside)),
                       hedge->getNodes(),
                       node->getParent()->get<std::string>("link-id") );
        }
      }

    if( modified )
      _dirty |= VISIBLITY;
  }

  //----------------------------------------------------------------------------
  bool ClientInfo::updateNode(
    LinkDescription::Node& node,
    const QRect& desktop, // in local
    const QRect& view,    // coordinates!
    const WindowRegions& windows,
    const WindowInfos::const_iterator& first_above
  )
  {
    if( node.getVertices().empty() )
      return false;

    bool modified = false;
    QPoint center_rel = node.getCenter().toQPoint(),
           center_abs = center_rel
                      + getScrollRegionAbs().topLeft();

    bool onscreen   = desktop.contains(center_rel),
         covered    = windows.hit(first_above, center_abs),
         outside    = !view.contains(center_rel),
         hidden     =   !onscreen
                    || (!_ipc_server->getOutsideSeeThrough() && outside);

    modified |= node.set("on-screen", onscreen);
    modified |= node.set("covered", covered);
    modified |= node.set("outside", outside);
    modified |= node.set("hidden", hidden);

    return modified;
  }

  //----------------------------------------------------------------------------
  bool ClientInfo::updateChildren(
    LinkDescription::HyperEdge& hedge,
    const QRect& desktop,
    const QRect& view,
    const WindowRegions& windows,
    const WindowInfos::const_iterator& first_above
  )
  {
    bool modified = false;
    for(auto node: hedge.getNodes())
      modified |= updateNode(*node, desktop, view, windows, first_above);
    return modified;
  }

  //------------------------------------------------------------------------------
  void ClientInfo::updateHedges( LinkDescription::hedges_t& hedges,
                                 bool first )
  {
    for(auto& hedge: hedges)
    {
      hedge->set("client_wid", _window_info.id);
      hedge->set( "screen-offset", _window_info.minimized
                                 ? QPoint()
                                 : getScrollRegionAbs().topLeft() );

      if( first )
      {
        hedge->set("partitions_src", to_string(tile_map->partitions_src));
        hedge->set("partitions_dest", to_string(tile_map->partitions_dest));
      }

      for(auto& node: hedge->getNodes())
        updateHedges(node->getChildren(), false);
    }
  }

  //----------------------------------------------------------------------------
  void ClientInfo::updateTileMap()
  {
    assert(_ipc_server);

    const bool do_partitions = true;
    const bool partition_compress = true;
    const double preview_width = _ipc_server->getPreviewWidth(),
                 preview_height = _ipc_server->getPreviewHeight();

    preview_size = scroll_region.size();

    Partitions partitions_src,
               partitions_dest;

    if( do_partitions )
    {
      PartitionHelper part;
      for(auto const& node: _nodes)
        for(auto const& hedge: node->getChildren())
          for(auto const& region: hedge->getNodes())
          {
            const Rect& bb = region->getBoundingBox();
            part.add( float2( bb.t() - 3 * _avg_region_height,
                              bb.b() + 3 * _avg_region_height ) );
          }

      part.clip(0, scroll_region.height(), 2 * _avg_region_height);
      partitions_src = part.getPartitions();

      if( partition_compress )
      {
        const int compress_size = _avg_region_height * 1.5;
        float cur_pos = 0;
        for( auto part = partitions_src.begin();
                  part != partitions_src.end();
                ++part )
        {
          if( part->x > cur_pos )
            cur_pos += compress_size;

          float2 target_part;
          target_part.x = cur_pos;

          cur_pos += part->y - part->x;
          target_part.y = cur_pos;

          partitions_dest.push_back(target_part);
        }

        if( partitions_src.back().y + 0.5 < scroll_region.height() )
          cur_pos += compress_size;

        int min_height = static_cast<float>(preview_height)
                       / preview_width
                       * scroll_region.width() + 0.5;
        cur_pos = std::max<int>(min_height, cur_pos);

        preview_size.setHeight(cur_pos);
      }
      else
        partitions_dest = partitions_src;
    }
    else
    {
      partitions_src.push_back(float2(0, scroll_region.height()));
      partitions_dest = partitions_src;
    }

    tile_map =
      std::make_shared<HierarchicTileMap>( preview_size.width(),
                                           preview_size.height(),
                                           preview_width,
                                           preview_height );

    tile_map->partitions_src = partitions_src;
    tile_map->partitions_dest = partitions_dest;

    for(auto& popup: _popups)
      popup->hover_region.tile_map = tile_map;
  }

} // namespace LinksRouting
