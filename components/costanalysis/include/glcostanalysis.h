#ifndef LR_GLCOSTANALYSIS
#define LR_GLCOSTANALYSIS

#include "costanalysis.h"
#include "common/componentarguments.h"
#include "slots.hpp"
#include "slotdata/image.hpp"

#include <string>

namespace LinksRouting
{
  class GlCostAnalysis: public CostAnalysis, public ComponentArguments
  {
    protected:
      std::string myname;
    public:
      GlCostAnalysis();
      virtual ~GlCostAnalysis();

      virtual void publishSlots(SlotCollector& slots);

      bool startup(Core* core, unsigned int type);
      void init();
      void shutdown();
      bool supports(Type type) const
      {
        return type == Component::Costanalysis;
      }
      const std::string& name() const
      {
        return myname;
      }

      void process(Type type);

//      bool setSceneInput(const Component::MapData& inputmap);
//      bool setCostreductionInput(const Component::MapData& inputmap);
      void connect(LinksRouting::Routing* routing);

      void computeColorCostMap(const Color& c);

    private:

      slot_t<SlotType::Image>::type _slot_costmap;

  };
} // namespace LinksRouting

#endif //LR_GLCOSTANALYSIS
