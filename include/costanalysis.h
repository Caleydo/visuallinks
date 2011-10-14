#ifndef LR_COSTANALYSIS
#define LR_COSTANALYSIS
#include <component.h>
#include <color.h>

namespace LinksRouting
{
  class Routing;
  class CostAnalysis : public virtual Component
  {
  public:
    virtual bool setSceneInput(const Component::MapData& inputmap) = 0;
    virtual bool setCostreductionInput(const Component::MapData& inputmap) = 0;
    virtual void connect(LinksRouting::Routing* routing) = 0;

    virtual void computeColorCostMap(const Color& c) = 0;
  };
};


#endif //LR_COSTANALYSIS