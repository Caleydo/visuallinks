#ifndef LR_COSTANALYSIS
#define LR_COSTANALYSIS
#include <component.h>
#include <color.h>

namespace LinksRouting
{
  class Routing;
  class CostAnalysis:
    public virtual Component
  {
    protected:
      CostAnalysis():
        Configurable("CostAnalysis")
      {}
  };
};


#endif //LR_COSTANALYSIS
