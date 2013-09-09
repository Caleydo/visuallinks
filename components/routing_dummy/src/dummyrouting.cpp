#include "dummyrouting.h"
#include "log.hpp"

namespace LinksRouting
{
  typedef LinkDescription::HyperEdgeDescriptionSegment segment_t;

  //----------------------------------------------------------------------------
  DummyRouting::DummyRouting():
    Configurable("NoRouting")
  {

  }

  //------------------------------------------------------------------------------
  void DummyRouting::publishSlots(SlotCollector& slots)
  {

  }

  //----------------------------------------------------------------------------
  void DummyRouting::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LinkDescription::LinkList>("/links");
  }

  //----------------------------------------------------------------------------
  uint32_t DummyRouting::process(unsigned int type)
  {
    if( !_subscribe_links->isValid() )
    {
      LOG_DEBUG("No valid routing data available.");
      return 0;
    }

    LinkDescription::LinkList& links = *_subscribe_links->_data;
    for( auto it = links.begin(); it != links.end(); ++it )
      collectNodes(it->_link.get());

    return RENDER_DIRTY | MASK_DIRTY;
  }

  //----------------------------------------------------------------------------
  void DummyRouting::collectNodes(LinkDescription::HyperEdge* hedge)
  {
    bool no_route = hedge->get<bool>("no-route");

    auto fork =
      std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>();
    hedge->setHyperEdgeDescription(fork);
    fork->position = hedge->getCenter();

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

      if( node->get<bool>("hidden") )
        continue;

      segment_t segment;
      segment.nodes.push_back(node);

      fork->outgoing.push_back(segment);

//      if( !node->get<std::string>("outside-scroll").empty() )
//        outside_nodes.push_back(node);
    }
  }

} // namespace LinksRouting
