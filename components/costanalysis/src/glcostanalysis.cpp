#include "glcostanalysis.h"
#include "slotdata/image.hpp"

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
  bool GlCostAnalysis::startup(Core* core, unsigned int type)
  {
    return true;
  }
  void GlCostAnalysis::init()
  {

  }
  void GlCostAnalysis::shutdown()
  {

  }

  void GlCostAnalysis::process(Type type)
  {
#if 0
    //just to get an idea how the saliency filtern should work -> copied from another project
    /////
    //load shaders
    featureMap.shader = shaderManager->loadfromFile(0, "featureMap.glsl");
    featureMapInput = featureMap.shader->GetUniformLocation("input0");

    saliencyMap.shader = shaderManager->loadfromFile(0, "saliencyFromFilter.glsl");
    saliencyMap.input[0] = saliencyMap.shader->GetUniformLocation("input0");
    saliencyMap.input[1] = saliencyMap.shader->GetUniformLocation("input1");
    saliencyMap.input[2] = saliencyMap.shader->GetUniformLocation("step");
    saliencyMap.input[3] = saliencyMap.shader->GetUniformLocation("texincrease");
    saliencyMap.input[4] = saliencyMap.shader->GetUniformLocation("scalesaliency");


    //do feature map (step 1)
    glPushAttrib(GL_VIEWPORT_BIT);
    glColor3f(1,1,1);
    featureMap.fbo->Bind();
    glViewport(0,0, featureMap.imgWidth, featureMap.imgHeight);
    featureMap.shader->begin();
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(featureMapInput, 0);
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_BLEND);
    glBegin(GL_QUADS);
    glTexCoord2f(u, v);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, v);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f - u, 1.0f - v);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(u, 1.0f - v);
    glVertex2f(-1.0f, 1.0f);
    glEnd();
    glPopAttrib();
    featureMap.shader->end();
    FramebufferObject::Disable();
    glPopAttrib();
     /////

    //do saliency filters step 2 (2 passes)
    GLint vp[4] = { 0, 0, 0, 0};
    glPushAttrib(GL_VIEWPORT_BIT);
      glGetIntegerv(GL_VIEWPORT, vp);
    if(vp[0] > 0)
      vp[2] -= vp[0], vp[0] = 0;
    if(vp[1] > 0)
      vp[3] -= vp[1], vp[1] = 0;
    if(vp[2] > tempSaliencyMap.imgWidth)
    {
      std::cerr << "Warning saliency map width to small for requested output dimension ("<<vp[2] <<" > " << tempSaliencyMap.imgWidth << "), reallocating temp map" << std::endl;
      setupTexture(true, tempSaliencyMap, vp[2], tempSaliencyMap.imgHeight, false, false, 2);
    }

    //first pass
    tempSaliencyMap.fbo->Bind();
    GLenum  b[2] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};
    glDrawBuffers(2, b);
    glViewport(0, 0, vp[2], featureMap.imgHeight);
    saliencyMap.shader->begin();
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, featureMap.id);
    glUniform1i(saliencyMap.input[0], 0);
    glUniform1i(saliencyMap.input[1], 1);
    glUniform1i(saliencyMap.input[2], 0);
    glUniform2f(saliencyMap.input[3], 1.0f/featureMap.imgWidth, 1.0f/featureMap.imgHeight);
    float saliencyscale = VOLUME_RENDERING?1.5f:(TEXTURED_OBJECT?1.0f:3.5f);
    glUniform1f(saliencyMap.input[4], saliencyscale);
    featureMap.drawQuad();

    FramebufferObject::Disable();
    glPopAttrib();


    //second pass
    glDrawBuffers(1, b);
    glBindTexture(GL_TEXTURE_2D, tempSaliencyMap.id);
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tempSaliencyMap.additionalTextures[0]);
    glUniform1i(saliencyMap.input[2], 1);
    glUniform2f(saliencyMap.input[3], 1.0f/tempSaliencyMap.imgWidth, 1.0f/tempSaliencyMap.imgHeight);

    float u0 = tempSaliencyMap.u0, u1 = static_cast<float>(vp[2])/tempSaliencyMap.imgWidth, v0 = tempSaliencyMap.v0, v1 = static_cast<float>(featureMap.imgHeight)/tempSaliencyMap.imgHeight;
    glBegin(GL_QUADS);
      glTexCoord2f(u0, v0); glVertex3f(-1, -1, 0);
      glTexCoord2f(u1, v0); glVertex3f( 1, -1, 0);
      glTexCoord2f(u1, v1); glVertex3f( 1,  1, 0);
      glTexCoord2f(u0, v1); glVertex3f(-1,  1, 0);
    glEnd();

    saliencyMap.shader->end();
    glBindTexture(GL_TEXTURE_2D,0);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,0);
    glDisable(GL_TEXTURE_2D);
#endif


  }

  void GlCostAnalysis::connect(LinksRouting::Routing* routing)
  {

  }

  void GlCostAnalysis::computeColorCostMap(const Color& c)
  {

  }
};
