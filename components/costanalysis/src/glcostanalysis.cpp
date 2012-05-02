#include "glcostanalysis.h"
#include "slotdata/image.hpp"

#include <stdexcept>

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  GlCostAnalysis::GlCostAnalysis():
    myname("GLCostAnalysis")
  {
    registerArg("DownsampleCost", _downsample = 2);
  }

  //----------------------------------------------------------------------------
  GlCostAnalysis::~GlCostAnalysis()
  {

  }

  //------------------------------------------------------------------------------
  void GlCostAnalysis::publishSlots(SlotCollector& slots)
  {
    _slot_costmap = slots.create<SlotType::Image>("/costmap");
    _slot_featuremap = slots.create<SlotType::Image>("/featuremap");
    _slot_downsampledinput = slots.create<SlotType::Image>("/downsampled_desktop");
  }

  //----------------------------------------------------------------------------
  void GlCostAnalysis::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_desktop =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/desktop");
  }

  //----------------------------------------------------------------------------
  bool GlCostAnalysis::startup(Core* core, unsigned int type)
  {
    return true;
  }
  void GlCostAnalysis::init()
  {

  }

  //----------------------------------------------------------------------------
  void GlCostAnalysis::initGL()
  {
    size_t width = _subscribe_desktop->_data->width / _downsample,
           height = _subscribe_desktop->_data->height / _downsample;
    _feature_map_fbo.init(width, height, GL_RGBA32F, 1, false, GL_NEAREST);
    _saliency_map_fbo.init(width, height, GL_R32F, 3, false, GL_NEAREST);
    _downsampled_input_fbo.init(width, height, GL_RGBA8, 1, false, GL_NEAREST);

    // TODO
    *_slot_costmap->_data = SlotType::Image(width, height, _saliency_map_fbo.colorBuffers.at(0));
    *_slot_downsampledinput->_data = SlotType::Image(width, height, _downsampled_input_fbo.colorBuffers.at(0));
    *_slot_featuremap->_data = SlotType::Image(width, height, _feature_map_fbo.colorBuffers.at(0));

    _feature_map_shader = _shader_manager.loadfromFile(0, "featureMap.glsl");
    _saliency_map_shader = _shader_manager.loadfromFile(0, "saliencyFilter.glsl");
    _downsample_shader = _shader_manager.loadfromFile(0, "downSample.glsl");
  }

  void GlCostAnalysis::shutdown()
  {

  }

  void GlCostAnalysis::process(Type type)
  {
    _slot_costmap->setValid(false);

    GLuint inputtex = _subscribe_desktop->_data->id;
    size_t width = _downsampled_input_fbo.width,
           height = _downsampled_input_fbo.height;

    //---------------------------------
    // downsample
    //---------------------------------

    if(_downsample > 0)
    {
      if( !_downsample_shader )
        throw std::runtime_error("Downsample shader not loaded.");

      _downsampled_input_fbo.bind();
      _downsample_shader->begin();
      _downsample_shader->setUniform1i("input0", 0);
      _downsample_shader->setUniform1i("samples", _downsample);
      _downsample_shader->setUniform2f("texincrease", 1.0f/_subscribe_desktop->_data->width, 1.0f/_subscribe_desktop->_data->height);

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, inputtex);

      glColor3f(1,1,1);
      _downsampled_input_fbo.draw(width, height, 0,0, -1, true, true);

       glDisable(GL_TEXTURE_2D);

      _downsample_shader->end();
      _downsampled_input_fbo.unbind();

      inputtex = _downsampled_input_fbo.colorBuffers.at(0);
    }

    //---------------------------------
    // do feature map (step 1)
    //---------------------------------

    if( !_feature_map_shader )
      throw std::runtime_error("Feature map shader not loaded.");

    _feature_map_fbo.bind();
    _feature_map_shader->begin();
    _feature_map_shader->setUniform1i("input0", 0);

    // glClear(GL_COLOR_BUFFER_BIT); TODO is this needed?

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, inputtex);

    glColor3f(1,1,1);
    _feature_map_fbo.draw(width, height, 0,0, -1, true, true);

    glDisable(GL_TEXTURE_2D);

    _feature_map_shader->end();
    _feature_map_fbo.unbind();

    //--------------------------------------
    // do saliency filters step 2 (2 passes)
    //--------------------------------------

    _saliency_map_fbo.bind();
    _saliency_map_shader->begin();

    //-----------
    // first pass


    _saliency_map_fbo.activateDrawBuffer(2, 1);

     _saliency_map_shader->setUniform1i("input0", 0);
     _saliency_map_shader->setUniform1i("input1", 1);
     _saliency_map_shader->setUniform1i("step", 0);
     _saliency_map_shader->setUniform2f("texincrease", 1.0f/width, 1.0f/height);

     _saliency_map_shader->setUniform1f("scalesaliency", 1.0f);


     glColor3f(1,1,1);
     _feature_map_fbo.draw(width, height, 0,0, 0, true, true);

    //------------
    // second pass

    _saliency_map_fbo.activateDrawBuffer(1);
    _saliency_map_fbo.bindTex(1);

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    _saliency_map_fbo.bindTex(2);

    _saliency_map_shader->setUniform1i("step", 1);

    glColor3f(1,1,1);
    _feature_map_fbo.draw(width, height, 0,0, -1, true, true);

    glDisable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);

    _saliency_map_shader->end();
    _saliency_map_fbo.unbind();


    _slot_costmap->setValid(true);
  }

  void GlCostAnalysis::connect(LinksRouting::Routing* routing)
  {

  }

  void GlCostAnalysis::computeColorCostMap(const Color& c)
  {

  }
};
