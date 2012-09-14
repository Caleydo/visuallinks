#ifndef LR_GLCOSTANALYSIS
#define LR_GLCOSTANALYSIS

#include "costanalysis.h"
#include "common/componentarguments.h"

#include "glsl/glsl.h"
#include "fbo.h"
#include "slots.hpp"
#include "slotdata/image.hpp"

#include <string>

namespace LinksRouting
{
  class GlCostAnalysis:
    public CostAnalysis,
    public ComponentArguments
  {
    protected:

      int _downsampleSaliency;
      int _downsampleCost;
      int _downsampleSaliencyToCost;

    public:

      GlCostAnalysis();
      virtual ~GlCostAnalysis();

      void publishSlots(SlotCollector& slots);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool startup(Core* core, unsigned int type);
      void init();
      bool initGL();
      void shutdown();
      bool supports(unsigned int type) const
      {
        return (type & Component::Costanalysis);
      }

      void process(unsigned int type);

//      bool setSceneInput(const Component::MapData& inputmap);
//      bool setCostreductionInput(const Component::MapData& inputmap);
      void connect(LinksRouting::Routing* routing);

      void computeColorCostMap(const Color& c);

    private:

      slot_t<SlotType::Image>::type _slot_costmap;
      slot_t<SlotType::Image>::type _slot_featuremap;
      slot_t<SlotType::Image>::type _slot_downsampledinput;
      slot_t<SlotType::Image>::type _subscribe_desktop;

      gl::FBO   _feature_map_fbo;
      gl::FBO   _saliency_map_fbo;
      gl::FBO   _cost_map_fbo;
      gl::FBO   _downsampled_input_fbo;

      cwc::glShaderManager  _shader_manager;
      cwc::glShader*    _feature_map_shader;
      cwc::glShader*    _saliency_map_shader;
      cwc::glShader*    _downsample_shader;

  };
} // namespace LinksRouting

#endif //LR_GLCOSTANALYSIS
