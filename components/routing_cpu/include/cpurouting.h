#ifndef LR_CPUROUTING
#define LR_CPUROUTING

#include "routing.h"
#include "common/componentarguments.h"

#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/polygon.hpp"

namespace LinksRouting
{
  class CPURouting: public Routing, public ComponentArguments
  {
    protected:

      std::string myname;

    public:

      CPURouting();

      void publishSlots(SlotCollector& slots);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool startup(Core* core, unsigned int type);
      void init();
      void shutdown();
      bool supports(unsigned int type) const
      {
        return (type & Component::Routing);
      }
      const std::string& name() const
      {
        return myname;
      }

      void process(unsigned int type);

    private:

      slot_t<LinkDescription::LinkList>::type _subscribe_links;
      float2 route(LinkDescription::HyperEdge* hedge);

  };
}
;
#endif //LR_CPUROUTING
