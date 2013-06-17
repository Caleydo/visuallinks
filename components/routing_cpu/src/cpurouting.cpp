#include "cpurouting.h"
#include "log.hpp"

#include <limits>

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


  LinkDescription::NodePtr buildIcon( float2 center,
                                      const float2& normal,
                                      bool triangle )
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
    //node->set("covered", true);
    //node->set("filled", true);

    return node;
  }

  /**
   * Get intersection/hide behind icon from inside to outside
   */
  LinkDescription::NodePtr getInsideIntersect( const float2& p_out,
                                               const float2& p_cov,
                                               const Rect& reg,
                                               bool triangle = false )
  {
    float2 dir = (p_out - p_cov).normalize();

    float2 normal;
    float fac = -1;

    if( std::fabs(dir.x) > 0.01 )
    {
      // if not too close to vertical try intersecting left or right
      if( dir.x > 0 )
      {
        fac = (reg.l() - p_cov.x) / dir.x;
        normal.x = 1;
      }
      else
      {
        fac = (reg.r() - p_cov.x) / dir.x;
        normal.x = -1;
      }

      // check if vertically inside the region
      float y = p_cov.y + fac * dir.y;
      if( y < reg.t() || y > reg.b() )
        fac = -1;
    }

    if( fac < 0 )
    {
      // nearly vertical or horizontal hit outside of vertical region
      // boundaries -> intersect with top or bottom
      normal.x = 0;

      if( dir.y < 0 )
      {
        fac = (reg.b() - p_cov.y) / dir.y;
        normal.y = -1;
      }
      else
      {
        fac = (reg.t() - p_cov.y) / dir.y;
        normal.y = 1;
      }
    }

    float2 new_center = p_cov + fac * dir;
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
                                                const Rect& reg,
                                                bool triangle = false )
  {
    float2 dir = (p_out - p_cov).normalize();

    float2 normal;
    float fac = -1;

    if( std::fabs(dir.x) > 0.01 )
    {
      // if not too close to vertical try intersecting left or right
      if( dir.x > 0 )
      {
        fac = (reg.r() - p_cov.x) / dir.x;
        normal.x = 1;
      }
      else
      {
        fac = (reg.l() - p_cov.x) / dir.x;
        normal.x = -1;
      }

      // check if vertically inside the region
      float y = p_cov.y + fac * dir.y;
      if( y < reg.t() || y > reg.b() )
        fac = -1;
    }

    if( fac < 0 )
    {
      // nearly vertical or horizontal hit outside of vertical region
      // boundaries -> intersect with top or bottom
      normal.x = 0;

      if( dir.y < 0 )
      {
        fac = (reg.t() - p_cov.y) / dir.y;
        normal.y = -1;
      }
      else
      {
        fac = (reg.b() - p_cov.y) / dir.y;
        normal.y = 1;
      }
    }

    float2 new_center = p_cov + fac * dir;
    float cx = (p_out.x + p_cov.x) / 2,
          cy = (p_out.y + p_cov.y) / 2;

    float2 positions[5] = {
      new_center,
      float2(cx, reg.t()),
      float2(cx, reg.b()),
      float2(reg.l(), cy),
      float2(reg.r(), cy)
    };

    float2 normals[5] = {
      normal,
      float2(0, -1),
      float2(0, 1),
      float2(-1, 0),
      float2(1, 0)
    };

    float min_dist = std::numeric_limits<float>::max();
    int min_index = 0;
    for(int i = 0; i < 5; ++i)
    {
      float dist = (p_out - positions[i]).length()
                 + (p_cov - positions[i]).length();

      if( dist < min_dist )
      {
        min_index = i;
        min_dist = dist;
      }
    }

    float2 min_pos = positions[min_index],
           min_norm = normals[min_index];

    // Prevent non optimal routes if direct route is shorter than midpoint route
    if( min_norm.y != 0 )
      autoClamp(min_pos.x, p_out.x, p_cov.x);
    else
      autoClamp(min_pos.y, p_out.y, p_cov.y);

    return buildIcon(min_pos, min_norm, triangle);
  }

  /**
   * Compare hedge segments by angle
   */
  struct CPURouting::cmp_by_angle
  {
    bool operator()( CPURouting::segment_iterator const& lhs,
                     CPURouting::segment_iterator const& rhs ) const
    {
      return getAngle(lhs) < getAngle(rhs);
    }

    static float getAngle( CPURouting::segment_iterator const& s)
    {
      if( s->trail.size() < 2 )
        return 0;
      const float2 dir = s->trail.at(1) - s->trail.at(0);
      return std::atan2(dir.y, dir.x);
    }
  };

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
  };

  typedef LinkDescription::HyperEdgeDescriptionSegment segment_t;

  //----------------------------------------------------------------------------
  CPURouting::CPURouting() :
    Configurable("CPURouting"),
    _global_num_nodes(0)
  {
    registerArg("SegmentLength", _initial_segment_length = 30);
    registerArg("NumIterations", _initial_iterations = 32);
    registerArg("NumSteps", _num_steps = 5);
    registerArg("NumSimplify", _num_simplify = std::numeric_limits<int>::max());
    registerArg("NumLinear", _num_linear = 1);
    registerArg("StepSize", _initial_step_size = 0.1);
    registerArg("SpringConstant", _spring_constant = 20);
    registerArg("AngleCompatWeight", _angle_comp_weight = 0.3);
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
  void CPURouting::process(unsigned int type)
  {
    if( !_subscribe_links->isValid() )
    {
      LOG_DEBUG("No valid routing data available.");
      return;
    }

    LinkDescription::LinkList& links = *_subscribe_links->_data;
    for( auto it = links.begin(); it != links.end(); ++it )
    {
      updateCenter(it->_link.get());

#ifndef GLOBAL_ROUTING
      route(it->_link.get());
#else
      _global_route_nodes.clear();
      _global_center = float2();
      _global_num_nodes = 0;

      routeGlobal(it->_link.get());

      if( _global_num_nodes )
      _global_center /= _global_num_nodes;

      OrderedSegments segments;
      for(const auto& group: _global_route_nodes)
        for(const auto& node: group.second)
        {
          auto const& p = node->getParent();
          if( !p )
            continue;

          auto const& fork = p->getHyperEdgeDescription();
          if( !fork )
            continue;

          float2 offset = p->get<float2>("screen-offset");

          segment_t segment;
          segment.set("covered", node->get<bool>("covered"));
          segment.set("widen-end", node->get<bool>("widen-end", true));
          segment.nodes.push_back(node);
          segment.trail.push_back(_global_center);
          segment.trail.push_back(offset + node->getBestLinkPoint(_global_center - offset) );

          segments.insert(
            fork->outgoing.insert(fork->outgoing.end(), segment)
          );
        }

      routeForceBundling(segments);
#endif

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
  bool CPURouting::updateCenter( LinkDescription::HyperEdge* hedge,
                                 float2* center_out )
  {
    float2 offset = hedge->get<float2>("screen-offset");
    LinkDescription::node_vec_t nodes;

    float2 center;
    size_t num_visible = 0;

    // add regions
    for( auto& node: hedge->getNodes() )
    {
      if( node->get<bool>("hidden") )
        continue;

      // add children (hyperedges)
      for( auto& child: node->getChildren() )
      {
        float2 child_center;
        if( updateCenter(child.get(), &child_center) )
        {
          center += child_center;
          num_visible += 1;
        }
        nodes.push_back(node);
      }

      if( !node->getVertices().empty() )
        nodes.push_back(node);
    }

    if( !num_visible )
    {
      for(auto const& group: regionGroupsByCoverWindow(nodes))
      {
        bool link_covered = group.first != 0;
        assert( !group.second.empty() );

        float2 group_center;
        int weight = link_covered ? 1 : 100;

        size_t group_num_visible = 0;
        for(size_t node_id: group.second)
        {
          auto const& node = nodes[ node_id ];
          if( !node->get<bool>("outside") )
          {
            group_center += node->getCenter();
            group_num_visible += 1;
          }
        }

        if( group_num_visible )
        {
          group_center /= group_num_visible;

          center += weight * group_center;
          num_visible += weight;
        }

        // TODO bias to hedge center
      }
    }

    if( num_visible )
      center /= num_visible;
    hedge->setCenter(center);

    if( center_out )
      *center_out = center + offset;
    return num_visible > 0;
  }

  //----------------------------------------------------------------------------
  void CPURouting::route(LinkDescription::HyperEdge* hedge)
  {
    bool no_route = hedge->get<bool>("no-route");

    auto fork =
      std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>();
    hedge->setHyperEdgeDescription(fork);
    fork->position = hedge->getCenter();

    std::vector<LinkDescription::points_t> regions;
    LinkDescription::node_vec_t nodes,
                                outside_nodes;
    float2 offset = hedge->get<float2>("screen-offset");

    // add regions
    for( auto& node: hedge->getNodes() )
    {
      if(    node->get<bool>("hidden")
          || (no_route && !node->get<bool>("always-route")) )
        continue;

      // add children (hyperedges)
      for( auto& child: node->getChildren() )
      {
        route(child.get());

        LinkDescription::points_t center(1);
        center[0] = child->getCenterAbs();
        regions.push_back(center);
        nodes.push_back(node);
      }

      if( node->getVertices().empty() )
        continue;

      if( !node->get<std::string>("outside-scroll").empty() )
        outside_nodes.push_back(node);

#if 0
      if( node->get<bool>("outside") )
      {
        Rect cover_reg = node->get<Rect>("covered-region") - offset;
        //auto icon = getInsideIntersect(fork->position, min_vert, cover_reg);
#if 0
//        auto& node = nodes[ group.second.at(0) ];
        Rect covering_reg = node->get<Rect>("covering-region") - offset;

        segment.trail.push_back( icon->getLinkPoints().front() );
        segment.nodes.push_back(icon);
        segment.set("widen-end", false);
#endif
      }
#endif
      regions.push_back(node->getLinkPoints());
      nodes.push_back(node);
    }

    // Create route for each group of regions covered by the same window
    auto region_groups = regionGroupsByCoverWindow(nodes);

    if( hedge->getParent() )
    {
      auto p = hedge->getParent()->getParent();
      if( p )
      {
        if(    fork->position == float2(0,0)
               // If there is only one group of regions we do not need to
               // connect them to the window center but can instead directly
               // connect it to the global center
            || region_groups.size() == 1 )
          fork->position = p->getCenter() - offset;
        else
          fork->position = 0.5 * (fork->position + p->getCenter() - offset);

        hedge->setCenter(fork->position);
      }
    }

    for(auto const& group: region_groups)
    {
      bool link_covered = group.first != 0;

      assert( !group.second.empty() );
      float2 center;

      std::set<segment_iterator, cmp_by_angle> group_segments;

      if( link_covered )
      {
        size_t num_visible = 0;
        for(size_t node_id: group.second)
        {
          auto const& node = nodes[node_id];
          int weight = node->get<bool>("outside") ? 1 : 100;
          center += weight * node->getCenter();
          num_visible += weight;
        }

        if( num_visible )
          center /= num_visible;
        // TODO bias to hedge center
      }
      else
      {
        center = fork->position;
      }

      for(size_t node_id: group.second)
      {
        auto &reg = regions[ node_id ];
        float2 min_vert;
        float min_dist = std::numeric_limits<float>::max();

        for( auto p = reg.begin(); p != reg.end(); ++p )
        {
          float dist = (*p - center).length();
          if( dist < min_dist )
          {
            min_vert = *p;
            min_dist = dist;
          }
        }

        auto& node = nodes[ node_id ];
        segment_t segment;
        segment.nodes.push_back(node);

        bool outside = node->get<bool>("outside");
        if( outside )
        {
          float2 min_pos;
          float2 node_center = node->getCenter();
          float min_dist = std::numeric_limits<float>::max();

          for(auto const& out: outside_nodes)
          {
            const float2 pos = out->getLinkPointsChildren().front();
            float dist = (pos - node_center).length();
            if( dist < min_dist )
            {
              min_pos = pos;
              min_dist = dist;
            }
          }

          segment.trail.push_back(offset + min_pos);
//          segment.trail.push_back(min_vert);
          segment.set("covered", true);

          //fork->outgoing.push_back(segment);
        }
        else
        {
          segment.trail.push_back(offset + center);
          segment.set("covered", link_covered);
        }

        segment.trail.push_back(offset + min_vert);
        group_segments.insert(
          fork->outgoing.insert(fork->outgoing.end(), segment)
        );
      }

#if 1
      routeForceBundling(group_segments);
#else
      auto getLength = []( float2 const& center,
                           float2 const& bundle_point,
                           float2 const& p1,
                           float2 const& p2 )
      {
        return (p2 - bundle_point).length()
             + (p1 - bundle_point).length()
             + (bundle_point - center).length();
      };

      auto s1 = group_segments.begin(),
           s2 = ++group_segments.begin();
      for(; s2 != group_segments.end(); ++s1, ++s2)
      {
        if( normalizePi( cmp_by_angle::getAngle(*s2)
                       - cmp_by_angle::getAngle(*s1) ) > M_PI / 2 )
          continue;

        float2 const& p1 = (*s1)->trail.back(),
                      p2 = (*s2)->trail.back(),
                      center = (*s2)->trail.at(0);
        float2 const dir = ( (p1 - center).normalize()
                           + (p2 - center).normalize()
                           ).normalize();
        float2 bundle_point = center;

        float cur_cost = -1,
              prev_cost = -1;
        const float2 STEP = 3 * dir;
        for(;;)
        {
          prev_cost = cur_cost;
          cur_cost = getLength(center, bundle_point, p1, p2);

          if( prev_cost < 0 || cur_cost <= prev_cost )
            bundle_point += STEP;
          else if( prev_cost > 0 )
            break;
        }

        // Revert last step which lead to worse result
        bundle_point -= STEP;

        (*s1)->trail.at(0) = bundle_point;
        (*s2)->trail.at(0) = bundle_point;
        (*s2)->trail.insert((*s2)->trail.begin(), center);
      }
#endif
      // Connect to visible links
      if( link_covered )
      {
        segment_t segment;
#if 0
        auto& node = nodes[ group.second.at(0) ];
        Rect covering_reg = node->get<Rect>("covering-region") - offset;
        auto icon = getVisibleIntersect(fork->position, center, covering_reg);
        segment.nodes.push_back(icon);
#endif
        segment.trail.push_back(offset + center);
#if 0
        segment.trail.push_back( icon->getLinkPointsChildren().front() );
        segment.set("covered", true);
        segment.set("widen-end", false);

        fork->outgoing.push_back(segment);

        segment.trail.clear();
        segment.nodes.clear();

        segment.trail.push_back(icon->getLinkPoints().front());
#endif
        segment.trail.push_back(offset + fork->position);
        segment.set("covered", false);

        fork->outgoing.push_back(segment);
      }
    }
  }

  //----------------------------------------------------------------------------
  void CPURouting::routeGlobal(LinkDescription::HyperEdge* hedge)
  {
    bool no_route = hedge->get<bool>("no-route");

    auto fork =
      std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>();
    hedge->setHyperEdgeDescription(fork);
    fork->position = hedge->getCenter();

    LinkDescription::node_vec_t nodes,
                                outside_nodes;
    float2 offset = hedge->get<float2>("screen-offset");

    // add regions
    for( auto& node: hedge->getNodes() )
    {
//      std::cout << "check " << node.get() << " " << node->getCenter()
//                << " hover = " << node->get<bool>("hover")
//                << " on-screen = " << node->get<bool>("on-screen")
//                << " covered = " << node->get<bool>("covered")
//                << std::endl;

      bool hover_preview = node->get<bool>("hover")
                        && node->get<bool>("on-screen")
                        && node->get<bool>("covered");

      if( hover_preview )
        std::cout << "hoverPreview " << node.get() << std::endl;

      if(    (!hover_preview &&  node->get<bool>("hidden"))
          || ( no_route      && !node->get<bool>("always-route")) )
        continue;

      // add children (hyperedges)
      for( auto& child: node->getChildren() )
        routeGlobal(child.get());

      nodes.push_back(node);

      if( !node->get<std::string>("outside-scroll").empty() )
        outside_nodes.push_back(node);
    }

    // Create route for each group of regions outside into the same direction
    // Store all other nodes for later global routing

    std::map<size_t, OrderedSegments> outside_groups;
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
        _global_center += node->getCenter() + offset;
        _global_num_nodes += 1;
        continue;
      }

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
    }

    for(auto const& group: outside_groups)
      routeForceBundling(group.second, false);
  }

  //----------------------------------------------------------------------------
  void CPURouting::routeForceBundling( const OrderedSegments& sorted_segments,
                                       bool trim_root )
  {
    if( sorted_segments.empty() )
      return;

    SegmentIterators segments(sorted_segments.begin(), sorted_segments.end());

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

      if( step == 0 )
        // In the first step create subdivided segments approximately the given
        // size.
        for(auto& segment: segments)
        {
          assert( segment->trail.size() == 2 );

          float2 dir = segment->trail.back() - segment->trail.front();
          segment->trail.pop_back();
          size_t num_segments = dir.length() / _initial_segment_length + 0.5;
          float2 sub_dir = dir / num_segments;
          for(size_t i = 0; i < num_segments; ++i)
            segment->trail.push_back(segment->trail.back() + sub_dir);
        }
      else
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
                normalizePi( cmp_by_angle::getAngle(segments[i])
                           - cmp_by_angle::getAngle(segments[other_i]) );

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

    // -----------------------
    // Finally clean up routes

    if( trim_root )
    {
      size_t num_skip = 0;
      for(bool check = true; check;)
      {
        float2 ref_pos;
        for(size_t i = 0; i < segments.size(); ++i)
        {
          auto& trail = segments[i]->trail;
          if( num_skip >= trail.size() )
          {
            check = false;
            break;
          }

          if( i == 0 )
            ref_pos = segments[0]->trail[num_skip];
          else if( (trail[num_skip] - ref_pos).length() > 5 )
          {
            check = false;
            break;
          }
        }

        if( check )
          ++num_skip;
      }

      if( num_skip )
        num_skip -= 1;

      if( num_skip )
        for(size_t i = 0; i < segments.size(); ++i)
        {
          auto& trail = segments[i]->trail;
          trail.erase(trail.begin(), trail.begin() + num_skip);
        }
    }

#if 1
    int max_clean = std::min<int>(_num_simplify, segments.size());
    for(int i = 0; i < max_clean; ++i)
    {
      auto& trail = segments[i]->trail;
      for(int other_i = i + 1; other_i < max_clean; ++other_i)
      {
        auto& other_trail = segments[other_i]->trail;
        size_t max_trail = std::min(trail.size(), other_trail.size() - 1);
        for(size_t j = max_trail; j --> 1; --j)
          if( (other_trail[j] - trail[j]).length() < 5 )
          {
            for(size_t k = 0; k < j; ++k)
              other_trail[k] = trail[j];
            break;
          }
      }
    }
#endif
//    {
//      auto& trail = segments[i]->trail;
//      if( trail.size() < 3 )
//        continue;
//
//      for(int offset = -min_offset; offset <= max_offset; ++offset)
//      {
//        if( !offset )
//          continue;
//        int other_i = (i + offset) % segments.size();
//        auto& other_trail = segments[other_i]->trail;
//
//        size_t max_trail = std::min(trail.size(), other_trail.size() - 1);
//        for(size_t j = max_trail; j --> 1;)
//        {
//          if( trail[j] == trail[j - 1] || other_trail[j] == other_trail[j - 1] )
//            break;
//          if( (other_trail[j] - trail[j]).length() < 10 )
//            other_trail[j] = trail[j];
//        }
//      }
//    }
  }

} // namespace LinksRouting
