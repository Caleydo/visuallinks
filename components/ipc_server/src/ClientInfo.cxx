/*
 * ClientInfo.cxx
 *
 *  Created on: Apr 8, 2013
 *      Author: tom
 */

#include <QPoint>
#include "ClientInfo.hxx"

#include <cassert>

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  ClientInfo::ClientInfo(WId wid):
    _window_info(wid)
  {

  }

  //----------------------------------------------------------------------------
  void ClientInfo::setWindowId(WId wid)
  {
    _window_info.id = wid;
  }

  //----------------------------------------------------------------------------
  bool ClientInfo::updateWindow(const WindowRegions& windows)
  {
    WId wid = _window_info.id;
    auto window_info = std::find_if
    (
      windows.begin(),
      windows.end(),
      [wid](const WindowInfo& winfo)
      {
        return winfo.id == wid;
      }
    );

    if( window_info == windows.end() )
      return false;

    _window_info = *window_info;

    return true;
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
  }

  //----------------------------------------------------------------------------
  LinkDescription::NodePtr ClientInfo::parseRegions( const JSONParser& json )
  {
    std::cout << "Parse regions (" << _window_info.id<< ")" << std::endl;
    _avg_region_height = 0;

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

//    std::cout << "parseRegions:\n";
//    for(auto& node: nodes)
//      std::cout << node->getProps();
//    std::cout << std::endl;

    LinkDescription::HyperEdgePtr hedge =
      std::make_shared<LinkDescription::HyperEdge>(nodes);
    //hedge->set("client_wid", WIdtoStr(_window_info.id));
//    hedge->set("offset", client_info.scroll_region.top() - top_left.y());

    hedge->set("partitions_src", to_string(tile_map->partitions_src));
    hedge->set("partitions_dest", to_string(tile_map->partitions_dest));
    partitions_src = tile_map->partitions_src;
    partitions_dest = tile_map->partitions_dest;

    //updateHedge(_window_monitor.getWindows(), hedge.get());
    _node = std::make_shared<LinkDescription::Node>(hedge);
    return _node;
  }

  //----------------------------------------------------------------------------
  void ClientInfo::updateTileMap()
  {
    const bool do_partitions = true;
    const bool partition_compress = true;
    const double _preview_width = 400,
                 _preview_height = 800;

    preview_size = scroll_region.size();

    LinkDescription::HyperEdge* hedge = _node ? _node->getParent() : 0;
    if( !hedge )
      return;

    Partitions partitions_src,
               partitions_dest;

    if( do_partitions )
    {
      PartitionHelper part;
      for(auto const& node: hedge->getNodes())
      {
        const Rect& bb = node->getBoundingBox();
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

        int min_height = static_cast<float>(_preview_height)
                       / _preview_width
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
                                           _preview_width,
                                           _preview_height );

    tile_map->partitions_src = partitions_src;
    tile_map->partitions_dest = partitions_dest;
  }

  //----------------------------------------------------------------------------
  QRect ClientInfo::getViewportAbs() const
  {
    return viewport.translated( _window_info.region.topLeft() );
  }

  //----------------------------------------------------------------------------
  void ClientInfo::activateWindow()
  {
    if( _window_info.isValid() )
      QxtWindowSystem::activeWindow( _window_info.id );
  }

  //----------------------------------------------------------------------------
  LinkDescription::NodePtr ClientInfo::getNode()
  {
    return _node;
  }

} // namespace LinksRouting
