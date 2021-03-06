#ifndef LR_CPUROUTING
#define LR_CPUROUTING

#include "routing.h"
#include "common/componentarguments.h"

#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/polygon.hpp"

#ifndef QWINDOWDEFS_H
#ifdef _WIN32
# include <windows.h>
  typedef HWND WId;
#else
  typedef unsigned long WId;
#endif
#endif

namespace LinksRouting
{
  class CPURouting: public Routing, public ComponentArguments
  {
    public:

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

      int       _initial_segment_length,
                _initial_iterations,
                _num_steps,
                _num_simplify,
                _num_linear;
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

      void routeForceBundling( const OrderedSegments& segments,
                               bool trim_root = true );

  };
}
;
#endif //LR_CPUROUTING
