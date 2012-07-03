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
      bool supports(Type type) const
      {
        return type == Component::Routing;
      }
      const std::string& name() const
      {
        return myname;
      }

      void process(Type type);

    private:

      slot_t<LinkDescription::LinkList>::type _subscribe_links;

      bool _enabled;
  };
}
;
#endif //LR_CPUROUTING
