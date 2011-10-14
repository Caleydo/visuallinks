#include "cpurouting.h"

namespace LinksRouting
{
    CPURouting::CPURouting() : myname("CPURouting")
    {

    }

    bool CPURouting::startup(Core* core, unsigned int type)
    {
      return true;
    }
    void CPURouting::init()
    {

    }
    void CPURouting::shutdown()
    {

    }

    void CPURouting::process(Type type)
    {

    }

    bool CPURouting::setCostInput(const Component::MapData& inputmap)
    {
      return true;
    }
    bool CPURouting::setColorCostInput(const Component::MapData& inputmap)
    {
      return true;
    }
    void CPURouting::connect(CostAnalysis* costanalysis, LinksRouting::Renderer *renderer)
    {

    }

    bool CPURouting::addLinkHierarchy(LinkDescription::Node* node, double priority)
    {
      return true;
    }
    bool CPURouting::addLinkHierarchy(LinkDescription::HyperEdge* hyperedge, double priority)
    {
      return true;
    }
    bool CPURouting::removeLinkHierarchy(LinkDescription::Node* node)
    {
      return true;
    }
    bool CPURouting::removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge)
    {
      return true;
    }

}