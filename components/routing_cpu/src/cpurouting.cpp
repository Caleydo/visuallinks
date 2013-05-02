#include "cpurouting.h"
#include "log.hpp"

#include <limits>

#ifdef _WIN32
typedef HWND WId;
#else
typedef unsigned long WId;
#endif

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

    return buildIcon(positions[min_index], normals[min_index], triangle);
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
  float2 CPURouting::updateCenter(LinkDescription::HyperEdge* hedge)
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
        center += updateCenter(child.get());
        num_visible += 1;
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

    return center + offset;
  }

  //----------------------------------------------------------------------------
  void CPURouting::route(LinkDescription::HyperEdge* hedge)
  {
    auto fork =
      std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>();
    hedge->setHyperEdgeDescription(fork);

    std::vector<LinkDescription::points_t> regions;
    LinkDescription::node_vec_t nodes,
                                outside_nodes;

    float2 offset = hedge->get<float2>("screen-offset");
    fork->position = hedge->getCenter();

    if( hedge->getParent() )
    {
      auto p = hedge->getParent()->getParent();
      if( p )
      {
        if( fork->position == float2(0,0) )
          fork->position = p->getCenter() - offset;
        else
          fork->position = 0.5 * (fork->position + p->getCenter() - offset);

        hedge->setCenter(fork->position);
      }
    }

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
    for(auto const& group: regionGroupsByCoverWindow(nodes))
    {
      bool link_covered = group.first != 0;

      assert( !group.second.empty() );
      float2 center;

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
        LinkDescription::HyperEdgeDescriptionSegment segment;
        segment.nodes.push_back(node);

        if( node->get<bool>("outside") )
        {
          float2 min_vert;
          float2 node_center = node->getCenter();
          float min_dist = std::numeric_limits<float>::max();

          for(auto const& out: outside_nodes)
          {
            const float2 pos = out->getLinkPointsChildren().front();
            float dist = (pos - node_center).length();
            if( dist < min_dist )
            {
              min_vert = pos;
              min_dist = dist;
            }
          }

          segment.trail.push_back(min_vert);
          segment.set("covered", true);
        }
        else
        {
          segment.trail.push_back(center);
          segment.set("covered", link_covered);
        }

        segment.trail.push_back(min_vert);
        fork->outgoing.push_back(segment);
      }

      // Connect to visible links
      if( link_covered )
      {
        auto& node = nodes[ group.second.at(0) ];
        Rect covering_reg = node->get<Rect>("covering-region") - offset;
        auto icon = getVisibleIntersect(fork->position, center, covering_reg);

        LinkDescription::HyperEdgeDescriptionSegment segment;
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
