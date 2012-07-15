#include "cpurouting.h"
#include "log.hpp"

#include <limits>

namespace LinksRouting
{
  //----------------------------------------------------------------------------
  CPURouting::CPURouting() :
    myname("CPURouting")
  {
    registerArg("enabled", _enabled = true);
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
    if( !_enabled )
      return;

    if( !_subscribe_links->isValid() )
    {
      LOG_DEBUG("No valid routing data available.");
      return;
    }

    LinkDescription::LinkList& links = *_subscribe_links->_data;
    for( auto it = links.begin(); it != links.end(); ++it )
    {
//      auto info = _link_infos.find(it->_id);
//
//      if(    info != _link_infos.end()
//          && info->second._stamp == it->_stamp
//          && info->second._revision == it->_link.getRevision() )
//        continue;
//
//      LOG_INFO("NEW DATA to route: " << it->_id << " using cpu routing");
      // TODO move looping and updating to common router component

      LinkDescription::HyperEdgeDescriptionForkation* fork =
        new LinkDescription::HyperEdgeDescriptionForkation();
      it->_link.setHyperEdgeDescription(fork);

      size_t num_nodes = 0;
      for( auto node = it->_link.getNodes().begin();
           node != it->_link.getNodes().end();
           ++node )
      {
        if( node->getProps().find("hidden") != node->getProps().end() )
          continue;

        assert( !node->getVertices().empty() );

        float2 region_center;
        for( auto p = node->getVertices().begin();
             p != node->getVertices().end();
             ++p )
        {
          region_center += *p;
        }

        fork->position += region_center / node->getVertices().size();
        num_nodes += 1;
      }

      if( num_nodes )
        fork->position /= num_nodes;

      //copy routes to hyperedge
      for( auto node = it->_link.getNodes().begin();
           node != it->_link.getNodes().end();
           ++node )
      {
        if( node->getProps().find("hidden") != node->getProps().end() )
          continue;

        float2 min_vert;
        float min_dist = std::numeric_limits<float>::max();

        for( auto p = node->getVertices().begin();
             p != node->getVertices().end();
             ++p )
        {
          float dist = (*p - fork->position).length();
          if( dist < min_dist )
          {
            min_vert = *p;
            min_dist = dist;
          }
        }

        fork->outgoing.push_back(LinkDescription::HyperEdgeDescriptionSegment());
        fork->outgoing.back().parent = fork;

        if( num_nodes > 1 )
          // Only add route if at least one other node exists
          fork->outgoing.back().trail.push_back(min_vert);

        fork->outgoing.back().nodes.push_back(&*node);
      }
    }
  }
}
