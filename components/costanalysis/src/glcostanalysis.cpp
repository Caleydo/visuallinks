#include "glcostanalysis.h"
#include "slotdata/image.hpp"

#include <stdexcept>

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  GlCostAnalysis::GlCostAnalysis():
    myname("GlCostAnalysis")
  {

  }

  //----------------------------------------------------------------------------
  GlCostAnalysis::~GlCostAnalysis()
  {

  }

  //------------------------------------------------------------------------------
  void GlCostAnalysis::publishSlots(SlotCollector& slots)
  {
    _slot_costmap = slots.create<SlotType::Image>("/costmap");
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
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    _feature_map_fbo.init(vp[2], vp[3], GL_RGBA8, 1, false);
    _saliency_map_fbo.init(vp[2], vp[3], GL_RGBA8, 3, false);

    // TODO
    _slot_costmap->_data->id = _saliency_map_fbo.colorBuffers.at(0);
    _slot_costmap->_data->type = SlotType::Image::OpenGLTexture;
    _slot_costmap->_data->width = vp[2];
    _slot_costmap->_data->height = vp[3];

    _feature_map_shader = _shader_manager.loadfromFile(0, "featureMap.glsl");
    _saliency_map_shader = _shader_manager.loadfromFile(0, "saliencyFilter.glsl");
  }

  void GlCostAnalysis::shutdown()
  {

  }

  void GlCostAnalysis::process(Type type)
  {
    _slot_costmap->setValid(false);

    if( !_feature_map_shader )
      throw std::runtime_error("Feature map shader not loaded.");

    //---------------------------------
    // do feature map (step 1)
    //---------------------------------

    _feature_map_fbo.bind();
    _feature_map_shader->begin();
    _feature_map_shader->setUniform1i("input0", 0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _subscribe_desktop->_data->id);

    glBegin(GL_QUADS);
      glColor3f(1,1,1);

      glTexCoord2f(0,0);
      glVertex2f(-1,-1);

      glTexCoord2f(1,0);
      glVertex2f(1,-1);

      glTexCoord2f(1,1);
      glVertex2f(1,1);

      glTexCoord2f(0,1);
      glVertex2f(-1,1);

    glEnd();
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

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    _saliency_map_fbo.activateDrawBuffer(2, 1);

     glEnable(GL_TEXTURE_2D);
     _feature_map_fbo.bindTex(0);

     _saliency_map_shader->setUniform1i("input0", 0);
     _saliency_map_shader->setUniform1i("input1", 1);
     _saliency_map_shader->setUniform1i("step", 0);
     _saliency_map_shader->setUniform2f("texincrease", 1.0f/vp[2], 1.0f/vp[3]);

     const bool VOLUME_RENDERING = false,
                TEXTURED_OBJECT = false;
     float saliencyscale = VOLUME_RENDERING
                         ? 1.5f
                         : (TEXTURED_OBJECT ? 1.0f : 3.5f);
     _saliency_map_shader->setUniform1f("scalesaliency", saliencyscale);

     glBegin(GL_QUADS);
       glColor3f(1,1,1);

       glTexCoord2f(0,0);
       glVertex2f(-1,-1);

       glTexCoord2f(1,0);
       glVertex2f(1,-1);

       glTexCoord2f(1,1);
       glVertex2f(1,1);

       glTexCoord2f(0,1);
       glVertex2f(-1,1);
     glEnd();

    //------------
    // second pass

    _saliency_map_fbo.activateDrawBuffer(1);
    _saliency_map_fbo.bindTex(1);

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    _saliency_map_fbo.bindTex(2);

    _saliency_map_shader->setUniform1i("step", 1);

    glBegin(GL_QUADS);
      glColor3f(1,1,1);

      glTexCoord2f(0,0);
      glVertex2f(-1,-1);

      glTexCoord2f(1,0);
      glVertex2f(1,-1);

      glTexCoord2f(1,1);
      glVertex2f(1,1);

      glTexCoord2f(0,1);
      glVertex2f(-1,1);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0);

    // we need a smaller map for the routing process
    _saliency_map_fbo.bindTex(0);
    glGenerateMipmap(GL_TEXTURE_2D);

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
