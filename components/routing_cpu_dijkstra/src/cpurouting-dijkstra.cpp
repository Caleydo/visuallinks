#include "cpurouting-dijkstra.h"
#include "log.hpp"
#include "dijkstra.h"

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
      std::vector<dijkstra::Grid> grids
      (
        group.second.size(),
        dijkstra::Grid(grid_size.x, grid_size.y)
      );

      dijkstra::NodePos min_node{0,0, &grids[0]};
      size_t min_cost = dijkstra::Node::MAX_COST;
      float2 center;

      OrderedSegments segments;

      for(int phase = 0; phase < 2; ++phase )
      {
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

          if( phase == 0 )
          {
            auto const link_point = node->getLinkPoints().front() + offset;
            grids[i].run( std::min<size_t>(divdown(link_point.x, GRID_SIZE), grid_size.x - 1),
                          std::min<size_t>(divdown(link_point.y, GRID_SIZE), grid_size.y - 1) );
          }
          else
          {
            segment_t segment;
            segment.set("covered", node->get<bool>("covered") && !node->get<bool>("hover"));
            segment.set("widen-end", node->get<bool>("widen-end", true));
            segment.nodes.push_back(node);

            dijkstra::NodePos cur_node = min_node;
            cur_node.grid = &grids[i];

            do
            {
              segment.trail.push_back({ (cur_node.x + 0.5) * GRID_SIZE,
                                        (cur_node.y + 0.5) * GRID_SIZE });
              cur_node = cur_node.getParent();
            } while( cur_node->getCost() );

            segment.trail.back() = offset + node->getBestLinkPoint(center - offset);
            segment.trail = smooth(segment.trail, 0.4, 5);

//            segment.trail.push_back(center);
//            segment.trail.push_back(offset + node->getBestLinkPoint(center - offset) );

            segments.insert(
              fork->outgoing.insert(fork->outgoing.end(), segment)
            );
          }
        }

        if( phase == 0 )
        {
          for(size_t x = 0; x < grid_size.x; ++x)
            for(size_t y = 0; y < grid_size.y; ++y)
            {
              size_t cost = 0;
              for(auto const& grid: grids)
              {
                if( grid.hasRun() )
                  cost += grid(x, y).getCost();
              }

              if( cost < min_cost )
              {
                min_cost = cost;
                min_node.x = x;
                min_node.y = y;
              }
            }

          center.x = (min_node.x + 0.5) * GRID_SIZE;
          center.y = (min_node.y + 0.5) * GRID_SIZE;
        }
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

}
} // namespace LinksRouting
