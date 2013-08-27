#ifndef LR_CPUROUTING_DIJKSTRA
#define LR_CPUROUTING_DIJKSTRA

#include "routing.h"
#include "common/componentarguments.h"

#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/polygon.hpp"

#include <set>

#ifdef _WIN32
typedef HWND WId;
#else
typedef unsigned long WId;
#endif

namespace LinksRouting
{
namespace Dijkstra
{
  class CPURouting: public Routing, public ComponentArguments
  {
    public:

      typedef LinkDescription::HedgeSegmentList::iterator segment_iterator;
      typedef std::vector<segment_iterator> SegmentIterators;

      typedef std::map<WId, std::vector<LinkDescription::NodePtr>> RegionGroups;

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

      uint32_t process(unsigned int type) override;

    private:

      slot_t<LinkDescription::LinkList>::type _subscribe_links;

      /* Drawable desktop region */
      slot_t<Rect>::type _subscribe_desktop_rect;

      RegionGroups _global_route_nodes;

      void collectNodes(LinkDescription::HyperEdge* hedge);

  };
}
}

#endif //LR_CPUROUTING_DIJKSTRA
