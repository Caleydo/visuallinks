#ifndef LR_CPUROUTING
#define LR_CPUROUTING

#include <string>

#include <routing.h>
#include <common/componentarguments.h>

namespace LinksRouting
{
  class CPURouting : public Routing, public ComponentArguments
  {
  protected:
    std::string myname;
  public:
    CPURouting();

    bool startup(Core* core, unsigned int type);
    void init();
    void shutdown();
    bool supports(Type type) const
    {
      return type == Component::Routing;
    }
    const std::string& name() const
    {
      return myname;
    }

    void process(Type type);

    bool setCostInput(const Component::MapData& inputmap);
    bool setColorCostInput(const Component::MapData& inputmap);
    void connect(CostAnalysis* costanalysis, LinksRouting::Renderer *renderer);

    bool addLinkHierarchy(LinkDescription::Node* node, double priority);
    bool addLinkHierarchy(LinkDescription::HyperEdge* hyperedge, double priority);
    bool removeLinkHierarchy(LinkDescription::Node* node);
    bool removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge);


  };
};
#endif //LR_CPUROUTING