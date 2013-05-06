#include "cpurouting.h"
#include "log.hpp"

#include <limits>
#include <set>

#ifdef _WIN32
typedef HWND WId;
#else
typedef unsigned long WId;
#endif

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

  //----------------------------------------------------------------------------
  CPURouting::CPURouting() :
    Configurable("CPURouting")
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
      route(it->_link.get());

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

  typedef std::map<WId, std::vector<size_t>> RegionGroups;
  RegionGroups regionGroupsByCoverWindow(
    const LinkDescription::node_vec_t& nodes
  )
  {
    // Map window id to the regions they cover
    RegionGroups region_covers;
    for(size_t i = 0; i < nodes.size(); ++i )
    {
      WId covering_wid = nodes[i]->get<bool>("covered")
                       ? nodes[i]->get<WId>("covering-wid")
                       : 0;

      region_covers[ covering_wid ].push_back(i);
    }

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
    typedef LinkDescription::HyperEdgeDescriptionSegment segment_t;
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
      if( node->get<bool>("hidden") )
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

      regions.push_back(node->getLinkPoints());
      nodes.push_back(node);
    }

    // Create route for each group of regions covered by the same window
    RegionGroups region_groups = regionGroupsByCoverWindow(nodes);

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

      typedef decltype(fork->outgoing.begin()) segment_iterator;
      struct cmp_by_angle
      {
        bool operator()( segment_iterator const& lhs,
                         segment_iterator const& rhs ) const
        {
          return getAngle(lhs) < getAngle(rhs);
        }

        static float getAngle( segment_iterator const& s)
        {
          const float2 dir = s->trail.back() - s->trail.front();
          return std::atan2(dir.y, dir.x);
        }
      };
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

          segment.trail.push_back(min_pos);
          segment.trail.push_back(min_vert);
          segment.set("covered", true);

          fork->outgoing.push_back(segment);
        }
        else
        {
          segment.trail.push_back(center);
          segment.set("covered", link_covered);

          float2 dir = min_vert - segment.trail.front();
          size_t num_segments = outside ? 1 : dir.length() / 12 + 0.5;
          float2 sub_dir = dir / num_segments,
                 norm = sub_dir.normal() / 4;
          for(size_t i = 0; i < num_segments; ++i)
            segment.trail.push_back( segment.trail.back() + sub_dir + ((i & 1) ? -1 : 1) * norm );

          group_segments.insert(
            fork->outgoing.insert(fork->outgoing.end(), segment)
          );
        }
      }

#if 0
      auto getLength = []( float2 const& center,
                           float2 const& bundle_point,
                           float2 const& p1,
                           float2 const& p2 )
      {
        return (p2 - bundle_point).length()
             + (p1 - bundle_point).length()
             + (bundle_point - center).length();
      };

      auto normalizePi = [](float rad)
      {
        while(rad < 0)
          rad += M_PI;
        while(rad > M_PI)
          rad -= M_PI;
        return rad;
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
        auto& node = nodes[ group.second.at(0) ];
        Rect covering_reg = node->get<Rect>("covering-region") - offset;
        auto icon = getVisibleIntersect(fork->position, center, covering_reg);

        segment_t segment;
        segment.nodes.push_back(icon);
        segment.trail.push_back(center);
        segment.trail.push_back( icon->getLinkPointsChildren().front() );
        segment.set("covered", true);
        segment.set("widen-end", false);

        fork->outgoing.push_back(segment);

        segment.trail.clear();
        segment.nodes.clear();

        segment.trail.push_back(icon->getLinkPoints().front());
        segment.trail.push_back(fork->position);
        segment.set("covered", false);

        fork->outgoing.push_back(segment);
      }
    }
  }

} // namespace LinksRouting
