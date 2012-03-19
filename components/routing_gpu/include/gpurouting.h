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
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
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
      slot_t<SlotType::Image>::type _subscribe_desktop;
      slot_t<LinkDescription::LinkList>::type _subscribe_links;

      struct LinkInfo
      {
        uint32_t    _stamp;
        uint32_t    _revision;
        LinkInfo()
        {
        }
        LinkInfo(uint32_t  stamp, uint32_t revision) : _stamp(stamp), _revision(revision)
        {     
        }
      };

      //structs used in kernel
      typedef cl_int4 cl_QueueElement;
      struct cl_QueueGlobal
      {
        cl_uint front;
        cl_uint back;
        cl_int filllevel;
        cl_uint activeBlocks;
        cl_uint processedBlocks;
        cl_uint sortingBarrier;
        cl_int debug;
        cl_uint mincost;

        cl_QueueGlobal() { }
        cl_QueueGlobal(cl_uint _sortingBarrier) : 
          front(0),
          back(0),
          filllevel(0),
          activeBlocks(0),
          processedBlocks(0),
          sortingBarrier(_sortingBarrier),
          debug(0),
          mincost(0xFFFFFFFF)
        {
        }
      };

      std::map<std::string, LinkInfo>   _link_infos;

      cl::Context       _cl_context;
      cl::Device        _cl_device;
      cl::CommandQueue  _cl_command_queue;
      cl::Program       _cl_program;
      cl::Kernel        _cl_prepare_kernel;
      cl::Kernel        _cl_shortestpath_kernel;
      cl::Kernel        _cl_clearQueueLink_kernel;
      cl::Kernel        _cl_getMinimum_kernel;
      cl::Kernel        _cl_routeInOut_kernel;
      cl::Kernel        _cl_routeInterBlock_kernel;
      cl::Kernel        _cl_routeConstruct_kernel;

      int _blockSize[2];
      bool _enabled;

  };
} // namespace LinksRouting

#endif //LR_GPUROUTING
