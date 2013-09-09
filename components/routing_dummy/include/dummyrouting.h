#ifndef LR_DUMMYROUTING
#define LR_DUMMYROUTING

#include "routing.h"
#include "common/componentarguments.h"

#include "slots.hpp"

namespace LinksRouting
{
  class DummyRouting: public Routing, public ComponentArguments
  {
    public:
      DummyRouting();

      void publishSlots(SlotCollector& slots);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool supports(unsigned int type) const
      {
        return (type & Component::Routing);
      }

      uint32_t process(unsigned int type) override;

    protected:
      void collectNodes(LinkDescription::HyperEdge* hedge);

    private:
      slot_t<LinkDescription::LinkList>::type _subscribe_links;
  };
}

#endif //LR_DUMMYROUTING
