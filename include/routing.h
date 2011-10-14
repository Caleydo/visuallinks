#ifndef LR_ROUTING
#define LR_ROUTING
#include <component.h>
#include <linkdescription.h>

namespace LinksRouting
{
  class CostAnalysis;
  class Renderer;
  class Routing : public virtual Component
  {
  public:
    virtual bool setCostInput(const Component::MapData& inputmap) = 0;
    virtual bool setColorCostInput(const Component::MapData& inputmap) = 0;
    virtual void connect(CostAnalysis* costanalysis, LinksRouting::Renderer *renderer) = 0;

    virtual bool addLinkHierarchy(LinkDescription::Node* node, double priority) = 0;
    virtual bool addLinkHierarchy(LinkDescription::HyperEdge* hyperedge, double priority) = 0;
    virtual bool removeLinkHierarchy(LinkDescription::Node* node) = 0;
    virtual bool removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge) = 0;
  };
};


#endif //LR_ROUTING