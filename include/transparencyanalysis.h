#ifndef LR_TRANSPARENCYANALYSIS
#define LR_TRANSPARENCYANALYSIS
#include <component.h>
#include <color.h>

namespace LinksRouting
{
  class Renderer;
  class CostAnalysis;
  class TransparencyAnalysis : public virtual Component
  {
  public:
//    virtual bool setSceneInput(const Component::MapData& inputmap) = 0;
    virtual void connect(CostAnalysis* costanalysis, LinksRouting::Renderer* renderer) = 0;
  };
};


#endif //LR_COSTANALYSIS
