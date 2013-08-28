#include "cpurouting-dijkstra.h"
#include "log.hpp"

#include <array>
#include <cstring>
#include <limits>

// internal helper...
void smoothIteration( const std::vector<float2> in,
                      std::vector<float2>& out,
                      float smoothing_factor )
{
  auto begin = in.begin(),
       end = in.end();

  decltype(begin) p0 = begin,
                  p1 = p0 + 1,
                  p2 = p1 + 1;

  float f_other = 0.5f*smoothing_factor;
  float f_this = 1.0f - smoothing_factor;

  out.push_back(*p0);

  for(; p2 != end; ++p0, ++p1, ++p2)
    out.push_back(f_other * (*p0 + *p2) + f_this * (*p1));

  out.push_back(*p1);
}

/**
 * Smooth a line given by a list of points. The more iterations you choose the
 * smoother the curve will become. How smooth it can get depends on the number
 * of input points, as no new points will be added.
 *
 * @param points
 * @param smoothing_factor
 * @param iterations
 */
template<typename Collection>
std::vector<float2> smooth( const Collection& points,
                            float smoothing_factor,
                            unsigned int iterations )

{
  // now it's time to use a specific container...
  std::vector<float2> in(std::begin(points), std::end(points));

  if( in.size() < 3 )
    return in;

  std::vector<float2> out;
  out.reserve(in.size());

  smoothIteration( in, out, smoothing_factor );

  for(unsigned int step = 1; step < iterations; ++step)
  {
    in.swap(out);
    out.clear();
    smoothIteration(in, out, smoothing_factor);
  }

  return out;
}

template<typename T>
void clamp(T& val, T min, T max)
{
  val = std::max(min, std::min(val, max));
}

/**
 * Same as clamp but automatically detect which of the limits is min and max.
 */
template<typename T>
void autoClamp(T& val, T l1, T l2)
{
  if( l1 < l2 )
    clamp(val, l1, l2);
  else
    clamp(val, l2, l1);
}

namespace LinksRouting
{
namespace Dijkstra
{
  /**
   * Normalize angle to [-pi, pi]
   */
  float normalizePi(float rad)
  {
    while(rad < -M_PI)
      rad += 2 * M_PI;
    while(rad > M_PI)
      rad -= 2 * M_PI;
    return rad;
  }

  //----------------------------------------------------------------------------
  void routeForceBundling( const Routing::OrderedSegments& sorted_segments )
  {
    if( sorted_segments.empty() )
      return;

    const double _angle_comp_weight = 0.9,
                 _initial_iterations = 100,
                 _initial_step_size = 0.4,
                 _num_steps = 6,
                 _spring_constant = 300,
                 _num_linear = 0;

    Routing::SegmentIterators segments(sorted_segments.begin(), sorted_segments.end());

    // Force-Directed Edge Bundling
    // Danny Holten and Jarke J. van Wijk

    std::vector<std::vector<float2>> segment_forces(segments.size());

    int num_iterations = _initial_iterations;
    float step_size = _initial_step_size;

    int min_offset = std::min(4, (static_cast<int>(segments.size()) - 1) / 2),
        max_offset = std::min(4, static_cast<int>(segments.size()) - 1 - min_offset);

    for(int step = 0; step < _num_steps; ++step)
    {
      // Subdivide all segments to get smooth routes.

      if( step > 0 )
        // For all consecutive rounds double the number of segments
        for(auto& segment: segments)
        {
          auto& trail = segment->trail;
          size_t old_size = trail.size();
          trail.resize(old_size * 2 - 1);

          for(size_t i = old_size - 1; i > 0; --i)
          {
            trail[i * 2    ] = trail[i];
            trail[i * 2 - 1] = 0.5 * (trail[i] + trail[i - 1]);
          }
        }

      for(int iter = 0; iter < num_iterations; ++iter)
      {
        // Calculate forces
        for(int i = 0; i < static_cast<int>(segments.size()); ++i)
        {
          auto& trail = segments[i]->trail;
          if( trail.size() < 3 )
            continue;

          auto& forces = segment_forces[i];
          if( iter == 0 )
            forces.resize(trail.size() - 2);

          float len = (trail.back() - trail.front()).length();
          double spring_constant = _spring_constant / (len * trail.size());

          for(size_t j = 1; j < trail.size() - 1; ++j)
          {
            float2& force = forces[j - 1];
            force = spring_constant * (trail[j + 1] + trail[j - 1] - 2 * trail[j]);
            for(int offset = -min_offset; offset <= max_offset; ++offset)
            {
              if( !offset )
                continue;

              int other_i = (i + offset) % segments.size();
              float delta_angle =
                normalizePi( Routing::cmp_by_angle::getAngle(segments[i]->trail)
                           - Routing::cmp_by_angle::getAngle(segments[other_i]->trail) );

              if( std::fabs(delta_angle) > 0.7 * M_PI )
                continue;

  //              if( std::fabs(delta_angle) > (std::abs(offset) < 2 ? 0.7 : 0.35) * M_PI )
  //                continue;
  //              float dist_scale = float(j) / trail.size();
  //              if( std::fabs(delta_angle) * dist_scale * dist_scale > 0.1 * M_PI )
  //                continue;
  //
  //              if(    segments[i]->get<bool>("covered")
  //                  != segments[other_i]->get<bool>("covered") )
  //                continue;

              auto const& other_trail = segments[other_i]->trail;
              if( j >= other_trail.size() )
                continue;

              float trail_comp =
                  (1 - _angle_comp_weight)
                + _angle_comp_weight * std::max(0., cos(delta_angle));

              float2 dir = other_trail[j] - trail[j];
              float dist = dir.length();
              float f = 0;
              if( step < _num_linear )
                f = std::min(1./dist, 1.);
              else
                f = dist < 200 ? std::min(800 / (dist * dist), 1.f)
                               : 0;
              force += trail_comp * f * dir;
            }
          }
        }

        // Apply forces
        for(int i = 0; i < static_cast<int>(segments.size()); ++i)
        {
          auto& trail = segments[i]->trail;
          auto const& forces = segment_forces[i];

          for(size_t j = 1; j < trail.size() - 1; ++j)
            trail[j] += step_size * forces[j - 1];
        }
      }

      num_iterations = std::max<int>(num_iterations * 0.66, 5);
      step_size *= 0.5;
    }
  }

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

    const size_t GRID_SIZE = 48;
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
        _grids[i].run( std::min<size_t>(link_point.x, grid_size.x - 1),
                       std::min<size_t>(link_point.y, grid_size.y - 1) );
      }

      for(size_t x = 0; x < grid_size.x; ++x)
        for(size_t y = 0; y < grid_size.y; ++y)
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

      bundle(min_pos);

      float2 center = float2(min_pos.x + .5f, min_pos.y + .5f) * GRID_SIZE;
      OrderedSegments segments;

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
        segment.trail = smooth(segment.trail, 0.4, 2);

//            segment.trail.push_back(center);
//            segment.trail.push_back(offset + node->getBestLinkPoint(center - offset) );

        segments.insert(
          fork->outgoing.insert(fork->outgoing.end(), segment)
        );
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
      size_t costs[8][5] = {{0}},
             num_bundles[8] = {0},
             cur_cost = 0,
             cur_pos[8] = {0},
             min_cost = size_t(-1),
             min_pos[8] = {0};

      MiniumFinder( Outgoings const& outgoings,
                    Grids const& grids,
                    float2 const& pos )
      {
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
        }
      }
    };

    MiniumFinder min_finder(outgoings, _grids, float2(min_node.x, min_node.y));
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

//      std::cout << i << " -> " << dest << std::endl;

      float2 new_dir = offsetFromIndex(dest);
      for(auto const link: outgoings[i])
      {
        min_node.grid = &_grids[ link ];
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
