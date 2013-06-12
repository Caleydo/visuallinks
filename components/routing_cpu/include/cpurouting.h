#ifndef LR_CPUROUTING
#define LR_CPUROUTING

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
  class CPURouting: public Routing, public ComponentArguments
  {
    public:

      /**
       * Compare hedge segments by angle
       */
      struct cmp_by_angle;

      typedef LinkDescription::HedgeSegmentList::iterator segment_iterator;
      typedef std::vector<segment_iterator> SegmentIterators;
      typedef std::set<segment_iterator, cmp_by_angle> OrderedSegments;

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

      void process(unsigned int type);

    private:

      int       _initial_segment_length,
                _initial_iterations,
                _num_steps;
      double    _initial_step_size,
                _spring_constant,
                _angle_comp_weight;

      slot_t<LinkDescription::LinkList>::type _subscribe_links;
      RegionGroups _global_route_nodes;
      float2 _global_center;
      size_t _global_num_nodes;

      bool updateCenter( LinkDescription::HyperEdge* hedge,
                         float2* center = nullptr );
      void route(LinkDescription::HyperEdge* hedge);
      void routeGlobal(LinkDescription::HyperEdge* hedge);

      void routeForceBundling(const OrderedSegments& segments);

  };
}
;
#endif //LR_CPUROUTING
