#include "cpurouting-dijkstra.h"
#include "log.hpp"

#include <array>
#include <cstring>
#include <limits>

namespace LinksRouting
{
namespace Dijkstra
{
  typedef LinkDescription::HyperEdgeDescriptionSegment segment_t;

  //----------------------------------------------------------------------------
  CPURouting::CPURouting() :
    Configurable("CPURoutingDijkstra")
  {

  }

  //------------------------------------------------------------------------------
  void CPURouting::publishSlots(SlotCollector& slots)
  {

  }

  //----------------------------------------------------------------------------
  void CPURouting::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LinkDescription::LinkList>("/links");

    _subscribe_desktop_rect =
      slot_subscriber.getSlot<Rect>("/desktop/rect");
  }

  //----------------------------------------------------------------------------
  bool CPURouting::startup(Core* core, unsigned int type)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  void CPURouting::init()
  {

  }

  //----------------------------------------------------------------------------
  void CPURouting::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  float2 divdown(const float2& vec, size_t grid)
  {
    return float2( std::floor(vec.x / grid),
                   std::floor(vec.y / grid) );
  }

  int indexFromOffset(int dx, int dy)
  {
    // 0 1 2
    // 7   3
    // 6 5 4

    if( dy < 0 )
      return dx + 1;
    else if( dy > 0 )
      return 5 - dx;
    else if( dx != 0 )
      return dx < 0 ? 7 : 3;

    std::cerr << "invalid offset (" << dx << "|" << dy << ")" << std::endl;
    return -1;
  }

  float2 offsetFromIndex(int index)
  {
    index = index % 8;
    if( index < 3 )
      return float2(index - 1, -1);
    else if( index == 3 )
      return float2(1, 0);
    else if( index == 7 )
      return float2(-1, 0);
    else
      return float2(5 - index, 1);
  }

  size_t getCostForDir(const float2& dir)
  {
    return (dir.x == 0 || dir.y == 0) ? 2 : 3;
  }

  size_t getCost( const std::vector<size_t>& indices,
                  const float2& pos,
                  const std::vector<dijkstra::Grid>& grids )
  {
    dijkstra::NodePos node{ static_cast<size_t>(pos.x),
                            static_cast<size_t>(pos.y),
                            0 };

    size_t cur_cost = 0;
    for(size_t i: indices)
    {
      node.grid = const_cast<dijkstra::Grid*>(&grids[i]);
      cur_cost += node->getCost();
    }
    return cur_cost;
  }

#define GLOBAL_ROUTING
  //----------------------------------------------------------------------------
  uint32_t CPURouting::process(unsigned int type)
  {
    if( !_subscribe_links->isValid() )
    {
      LOG_DEBUG("No valid routing data available.");
      return 0;
    }

    const size_t GRID_SIZE = 32;
    float2 grid_size
    (
      divup(_subscribe_desktop_rect->_data->size.x, GRID_SIZE),
      divup(_subscribe_desktop_rect->_data->size.y, GRID_SIZE)
    );

    _global_route_nodes.clear();

    LinkDescription::LinkList& links = *_subscribe_links->_data;
    for( auto it = links.begin(); it != links.end(); ++it )
    {
      collectNodes(it->_link.get());
    }

    for(const auto& group: _global_route_nodes)
    {
      _grids.clear();
      _grids.resize
      (
        group.second.size(),
        dijkstra::Grid(grid_size.x, grid_size.y)
      );

      float2 min_pos;
      size_t min_cost = dijkstra::Node::MAX_COST;

      for(size_t pass = 0; pass < 2; ++pass)
        for(size_t i = 0; i < group.second.size(); ++i)
        {
          auto const& node = group.second[i];
          auto const& p = node->getParent();
          if( !p )
            continue;

          auto const& fork = p->getHyperEdgeDescription();
          if( !fork )
            continue;

          float2 offset = p->get<float2>("screen-offset");

          auto const link_point =
            divdown(node->getLinkPoints().front() + offset, GRID_SIZE);
          size_t x = std::min<size_t>(link_point.x, grid_size.x - 1),
                 y = std::min<size_t>(link_point.y, grid_size.y - 1);

          if( pass == 0 )
          {
            _grids[i].run(x, y);
          }
          else
          {
            size_t cost = 0;
            for(auto const& grid: _grids)
            {
              if( grid.hasRun() )
                cost += grid(x, y).getCost();
            }

            if( cost < min_cost )
            {
              min_cost = cost;
              min_pos = float2(x, y);
            }
          }
        }

      bundle(min_pos);

      float2 center = float2(min_pos.x + .5f, min_pos.y + .5f) * GRID_SIZE;

      for(size_t i = 0; i < group.second.size(); ++i)
      {
        if( !_grids[i].hasRun() )
          continue;

        auto const& node = group.second[i];
        auto const& p = node->getParent();
        auto const& fork = p->getHyperEdgeDescription();
        float2 const& offset = p->get<float2>("screen-offset");

        segment_t segment;
        segment.set("covered", node->get<bool>("covered") && !node->get<bool>("hover"));
        segment.set("widen-end", node->get<bool>("widen-end", true));
        segment.nodes.push_back(node);

        dijkstra::NodePos cur_node{ size_t(min_pos.x),
                                    size_t(min_pos.y),
                                    &_grids[i] };

        do
        {
          segment.trail.push_back({ (cur_node.x + .5f) * GRID_SIZE,
                                    (cur_node.y + .5f) * GRID_SIZE });
          cur_node = cur_node.getParent();
        } while( cur_node->getCost() );

        segment.trail.back() = offset + node->getBestLinkPoint(center - offset);
        segment.trail = smooth(segment.trail, 0.2, 2);

        for(size_t i = 0; i < 2; ++i)
        {
          subdivide(segment.trail);
          segment.trail = smooth(segment.trail, 0.4, 4);
        }

//            segment.trail.push_back(center);
//            segment.trail.push_back(offset + node->getBestLinkPoint(center - offset) );

        fork->outgoing.insert(fork->outgoing.end(), segment);
      }

      //routeForceBundling(segments);

//      auto info = _link_infos.find(it->_id);
//
//      if(    info != _link_infos.end()
//          && info->second._stamp == it->_stamp
//          && info->second._revision == it->_link.getRevision() )
//        continue;
//
//      LOG_INFO("NEW DATA to route: " << it->_id << " using cpu routing");
      // TODO move looping and updating to common router component
    }

    return RENDER_DIRTY | MASK_DIRTY;
  }

  WId getCoveringWId(const LinkDescription::Node& node)
  {
    return node.get<bool>("covered") ? node.get<WId>("covering-wid") : 0;
  }

  typedef std::map<WId, std::vector<size_t>> RegionGroups;
  RegionGroups regionGroupsByCoverWindow(
    const LinkDescription::node_vec_t& nodes
  )
  {
    // Map window id to the regions they cover
    RegionGroups region_covers;
    for(size_t i = 0; i < nodes.size(); ++i )
      region_covers[ getCoveringWId(*nodes[i]) ].push_back(i);
    return region_covers;
  }

  //----------------------------------------------------------------------------
  void CPURouting::collectNodes(LinkDescription::HyperEdge* hedge)
  {
    bool no_route = hedge->get<bool>("no-route");

    auto fork =
      std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>();
    hedge->setHyperEdgeDescription(fork);
    fork->position = hedge->getCenter();

    LinkDescription::node_vec_t nodes,
                                outside_nodes;
    //float2 offset = hedge->get<float2>("screen-offset");

    // add regions
    for( auto& node: hedge->getNodes() )
    {
      if(    node->get<bool>("hidden")
          || (no_route && !node->get<bool>("always-route")) )
        continue;

      // add children (hyperedges)
      for( auto& child: node->getChildren() )
        collectNodes(child.get());

      nodes.push_back(node);

      if( !node->get<std::string>("outside-scroll").empty() )
        outside_nodes.push_back(node);
    }

    // Create route for each group of regions outside into the same direction
    // Store all other nodes for later global routing

    //std::map<size_t, OrderedSegments> outside_groups;
    for(size_t i = 0; i < nodes.size(); ++i)
    {
      auto const& node = nodes[i];

      if( node->getVertices().empty() )
      {
        segment_t segment;
        segment.nodes.push_back(node);

        fork->outgoing.push_back(segment);
        continue;
      }

      if( !node->get<bool>("outside") )
      {
        _global_route_nodes[ getCoveringWId(*node) ].push_back(node);
        continue;
      }
    }

#if 0
      if( outside_nodes.empty() )
        continue;

      // get next outside node
      float2 node_center = node->getCenter();
      float min_dist = std::numeric_limits<float>::max();
      size_t min_index = -1;

      for(size_t j = 0; j < outside_nodes.size(); ++j)
      {
        const float2 pos = outside_nodes[j]->getLinkPointsChildren().front();
        float dist = (pos - node_center).length();
        if( dist < min_dist )
        {
          min_dist = dist;
          min_index = j;
        }
      }
      float2 min_pos =
        outside_nodes[min_index]->getLinkPointsChildren().front();

      segment_t segment;
      segment.set("covered", true);
      segment.nodes.push_back(node);
      segment.trail.push_back(offset + min_pos);
      segment.trail.push_back(offset + node->getBestLinkPoint(min_pos));

      outside_groups[ min_index ].insert(
        fork->outgoing.insert(fork->outgoing.end(), segment)
      );
#endif
    //}

//    for(auto const& group: outside_groups)
//      routeForceBundling(group.second, false);
  }

  //----------------------------------------------------------------------------
  void CPURouting::bundle( const float2& pos,
                           const Bundle& bundle )
  {
    typedef std::array<Bundle, 8> Outgoings;
    Outgoings outgoings;

    dijkstra::NodePos min_node{size_t(pos.x), size_t(pos.y), 0};

    if( bundle.empty() )
    {
      for(size_t i = 0; i < _grids.size(); ++i)
      {
        if( !_grids[i].hasRun() )
          continue;

        min_node.grid = &_grids[i];
        int index = indexFromOffset( min_node->getParentOffsetX(),
                                     min_node->getParentOffsetY() );
        if( index >= 0 )
          outgoings[ index ].push_back(i);
      }
    }
    else
    {
      for(auto const edge: bundle)
      {
        min_node.grid = &_grids[edge];
        if( min_node->getCost() == 0  )
          continue;

        int index = indexFromOffset( min_node->getParentOffsetX(),
                                     min_node->getParentOffsetY() );
        if( index >= 0 )
          outgoings[ index ].push_back( edge );
      }
    }

    struct MiniumFinder
    {
      size_t costs[8][5],
             num_bundles[8],
             cur_cost = 0,
             cur_pos[8],
             min_cost = size_t(-1),
             min_pos[8];

      MiniumFinder( Outgoings const& outgoings,
                    Grids const& grids,
                    float2 const& pos )
      {
        memset(num_bundles, 0, sizeof(num_bundles));
        memset(cur_pos, 0, sizeof(cur_pos));
        memset(min_pos, 0, sizeof(min_pos));

        // Precalculate costs for rotating the max 8 edge bundles by +-2
        // fields
        for(int i = 0; i < 8; ++i)
        {
          for(int offset = -2; offset <= 2; ++offset)
          {
            int other = (i + offset + 8) % 8;
            costs[i][offset + 2] = getCost
            (
              outgoings[i],
              pos + offsetFromIndex(other),
              grids
            );
          }

#if 0
          std::cout << "costs[" << i << "] = {";
          for(size_t j = 0; j < 5; ++j)
            std::cout << (j ? "," : "") << costs[i][j];
          std::cout << "}" << std::endl;
#endif
        }

      }

      void run(int bundle = 0)
      {
        for(size_t i = 0; i < 5; ++i)
        {
          size_t cost = costs[bundle][i];
          size_t cur_i = (i + bundle + 8 - 2) % 8;
          cur_pos[bundle] = cur_i;

          if( cost )
          {
            cur_cost += cost;
            num_bundles[cur_i] += 1;
          }

          if( bundle < 7 )
            run(bundle + 1);
          else
          {
            size_t bundle_costs = 0;
            for(size_t i = 0; i < 8; ++i)
              if( num_bundles[i] )
                bundle_costs += getCostForDir(offsetFromIndex(i));

            if( cur_cost + bundle_costs < min_cost )
            {
              min_cost = cur_cost + bundle_costs;
              memcpy(min_pos, cur_pos, sizeof(min_pos));
            }
          }

          if( cost )
          {
            cur_cost -= cost;
            num_bundles[cur_i] -= 1;
          }
          else
            break;
        }
      }
    };

    MiniumFinder min_finder(outgoings, this->_grids, float2(min_node.x, min_node.y));
    min_finder.run();

//    std::cout << "min_cost = " << min_finder.min_cost
//              << ", bundles: " << std::endl;

    Outgoings new_bundles;
    for(size_t i = 0; i < 8; ++i)
    {
      size_t const dest = min_finder.min_pos[i];
      if( !min_finder.costs[i][0] )
        continue;

      new_bundles[dest].insert(
        end(new_bundles[dest]),
        begin(outgoings[i]),
        end(outgoings[i])
      );

      if( dest == i )
        continue;

      //std::cout << i << " -> " << dest << std::endl;

      float2 new_dir = offsetFromIndex(dest);
      for(auto const link: outgoings[i])
      {
        min_node.grid = &this->_grids[ link ];
        min_node->setParentOffset(new_dir.x, new_dir.y);
      }
    }

    for(size_t i = 0; i < 8; ++i)
    {
      if( !new_bundles[i].empty() )
        this->bundle(pos + offsetFromIndex(i), new_bundles[i]);
    }
  }

}
} // namespace LinksRouting
