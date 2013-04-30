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
    return buildIcon(new_center, normal, triangle);
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

  //----------------------------------------------------------------------------
  float2 CPURouting::updateCenter(LinkDescription::HyperEdge* hedge)
  {
    float2 offset = hedge->get<float2>("screen-offset");

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
      }

      if( node->getVertices().empty() )
        continue;

      if(    !node->get<bool>("hidden")
          /*&& !node->get<bool>("covered")*/ )
      {
        center += node->getCenter();
        num_visible += 1;
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
    std::vector<LinkDescription::NodePtr> nodes;

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

      regions.push_back(node->getLinkPoints());
      nodes.push_back(node);
    }

    // Map window id to the regions they cover
    std::map<WId, std::vector<size_t>> region_covers;
    for(size_t i = 0; i < regions.size(); ++i )
    {
      WId covering_wid = nodes[i]->get<bool>("covered")
                       ? nodes[i]->get<WId>("covering-wid")
                       : 0;

      region_covers[ covering_wid ].push_back(i);
    }

    // Create route for each group of regions covered by the same window
    for(auto& group: region_covers)
    {
      assert( !group.second.empty() );
      float2 center;
      for(size_t node_id: group.second)
        center += nodes[ node_id ]->getCenter();
      center /= group.second.size();
      // TODO bias to hedge center

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
        bool outside = node->get<bool>("outside"),
             covered = node->get<bool>("covered");

        LinkDescription::HyperEdgeDescriptionSegment segment;
        //segment.parent = fork;
        segment.trail.push_back(center);

        if( outside || covered )
        {
          Rect cover_reg = node->get<Rect>("covered-region") - offset,
               covering_reg = node->get<Rect>("covering-region") - offset;
          auto icon = outside
                    ? getInsideIntersect(fork->position, min_vert, cover_reg)
                    : getVisibleIntersect(fork->position, min_vert, covering_reg);
          segment.trail.push_back( icon->getLinkPoints().front() );
          segment.nodes.push_back(icon);
          segment.set("widen-end", false);

          fork->outgoing.push_back(segment);

          segment.trail.clear();
          segment.nodes.clear();

          segment.trail.push_back(icon->getLinkPointsChildren().front());

          segment.set("covered", true);
        }

        segment.trail.push_back(min_vert);
        segment.nodes.push_back(node);

        fork->outgoing.push_back(segment);
      }
    }
  }

} // namespace LinksRouting
