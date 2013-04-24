#include "cpurouting.h"
#include "log.hpp"

#include <limits>

namespace LinksRouting
{
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
  float2 CPURouting::route(LinkDescription::HyperEdge* hedge)
  {
    auto fork =
      std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>();
    hedge->setHyperEdgeDescription(fork);

    std::vector<LinkDescription::points_t> regions;
    std::vector<LinkDescription::NodePtr> nodes;

    // add regions
    for( auto& node: hedge->getNodes() )
    {
      if( node->get<bool>("hidden") )
        continue;
//      if( (    (*node)->get<bool>("hidden", false)
//           && !(*node)->get<bool>("covered", false)
//          ) || (*node)->getParent() != hedge )
//        continue;

      // add children (hyperedges)
      for( auto& child: node->getChildren() )
      {
        LinkDescription::points_t center(1);
        center[0] = route(child.get());
        if( center[0] == float2(0,0) || child->get<bool>("no-parent-route") )
          continue;

        fork->position += center[0];
        regions.push_back(center);
        nodes.push_back(node);
      }

      if( node->getVertices().empty() )
        continue;

      fork->position += node->getCenter();
      regions.push_back(node->getLinkPoints());
      nodes.push_back(node);
    }

    if( !regions.empty() )
      fork->position /= regions.size();

    //fork->position = hedge->getCenter();

//    if( hedge->getParent() )
//    {
//      auto p = hedge->getParent()->getParent();
//      if( p )
//      {
//        if( fork->position == float2(0,0) )
//          fork->position = p->getCenter();
//        else
//          fork->position = 0.5 * (fork->position + p->getCenter());
//      }
//    }

    //copy routes to hyperedge
    for(size_t i = 0; i < regions.size(); ++i )
    {
      auto &reg = regions[i];
      float2 min_vert;
      float min_dist = std::numeric_limits<float>::max();

      for( auto p = reg.begin(); p != reg.end(); ++p )
      {
        float dist = (*p - fork->position).length();
        if( dist < min_dist )
        {
          min_vert = *p;
          min_dist = dist;
        }
      }

      fork->outgoing.push_back(LinkDescription::HyperEdgeDescriptionSegment());
      //fork->outgoing.back().parent = fork;

      //if( regions.size() > 1 || !hedge->getParent() )
        // Only add route if at least one other node exists
        fork->outgoing.back().trail.push_back(min_vert);

      fork->outgoing.back().nodes.push_back(nodes[i]);
    }

    return fork->position;
  }

} // namespace LinksRouting
