#ifndef LR_GPUROUTING
#define LR_GPUROUTING

#include "routing.h"
#include "common/componentarguments.h"

#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/polygon.hpp"

// Use Exceptions for OpenCL C++ API
#define __CL_ENABLE_EXCEPTIONS

#if defined(__APPLE__) || defined(__MACOSX)
# include <OpenCL/cl.hpp>
#else
# include <CL/cl.hpp>
#endif

#include <string>

namespace LinksRouting
{
  class GPURouting: public Routing, public ComponentArguments
  {
    protected:

      std::string myname;

    public:

      GPURouting();

      void publishSlots(SlotCollector& slots);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool startup(Core* core, unsigned int type);
      void init();
      void initGL();
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

//    bool setCostInput(const Component::MapData& inputmap);
//    bool setColorCostInput(const Component::MapData& inputmap);
      void connect(CostAnalysis* costanalysis,
                   LinksRouting::Renderer *renderer);

      bool addLinkHierarchy(LinkDescription::Node* node, double priority);
      bool addLinkHierarchy(LinkDescription::HyperEdge* hyperedge,
                            double priority);
      bool removeLinkHierarchy(LinkDescription::Node* node);
      bool removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge);

    private:

      template<typename T>
      static T divup(T a, T b)
      {
        return (a + b - 1)/b;
      }

      slot_t<SlotType::Image>::type _subscribe_costmap;
      slot_t<std::string>::type     _subscribe_search_id;
      slot_t<uint32_t>::type        _subscribe_search_stamp;
      slot_t<std::vector<SlotType::Polygon>>::type  _subscribe_search_regions;

      std::string   _last_search_id;
      uint32_t      _last_search_stamp;

      cl::Context       _cl_context;
      cl::Device        _cl_device;
      cl::CommandQueue  _cl_command_queue;
      cl::Program       _cl_program;
      cl::Kernel        _cl_prepare_kernel;
      cl::Kernel        _cl_shortestpath_kernel;
      cl::Kernel        _cl_clearQueueLink_kernel;

      int _blockSize[2];
      bool _enabled;

  };
}
;
#endif //LR_GPUROUTING
