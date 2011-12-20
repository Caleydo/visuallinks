#include "fbo.h"
#include <iostream>
#include <vector>
#include <stdexcept>

namespace gl
{
  FBO::FBO(unsigned int _width,
           unsigned int _height,
           unsigned int colorInternalFormat,
           unsigned int numColorBuffers,
           bool depthBuffer,
           unsigned int colorFilter,
           unsigned int depthFilter,
           bool supportMipMaps,
           bool stencilBuffer,
           unsigned int multisample) :
        _restore(false),
        width(_width),
        height(_height)
  {
    is_init = false;
    init(_width, _height, colorInternalFormat, numColorBuffers, depthBuffer,
         colorFilter, depthFilter, supportMipMaps, stencilBuffer, multisample);
  }
  void FBO::init(unsigned int _width,
                 unsigned int _height,
                 unsigned int colorInternalFormat,
                 unsigned int numColorBuffers,
                 bool depthBuffer,
                 unsigned int colorFilter,
                 unsigned int depthFilter,
                 bool supportMipMaps,
                 bool stencilBuffer,
                 unsigned int multisample)
  {
    if( is_init )
      return;
    glPushAttrib(GL_CURRENT_BIT);
#if !GL_ARB_texture_multisample || defined(NOMULTISAMPLING)
    if(multisample)
    //  compLogMessage("FBO", CompLogLevelWarn, "Init Fbo: no multisample support - disabling multisampling");
    multisample = 0;
#endif

    _colorInternalFormat = colorInternalFormat;
    _colorFilter = colorFilter;
    _depthFilter = depthFilter;
    _hasDepthBuffer = depthBuffer;
    _hasStencilBuffer = stencilBuffer;
    _multisample = multisample;

//  compLogMessage("FBO", CompLogLevelDebug, "Init Fbo");
    width = _width;
    height = _height;
    _restore = false;
    is_init = true;
    unsigned int temp_size = 0, temp_levels = 0;
    unsigned int i;
    GLint last;
    GLint lastrenderbuffer;
    GLenum status;

    if( numColorBuffers == 0 )
      numColorBuffers = 1;

    if( !supportMipMaps )
    {
      powOfTwoWidth = 0;
      powOfTwoHeight = 0;
      is_powOfTwo = false;
      mipmapLevels = mipmapLastWidth = mipmapLastHeight = 0;
      //  compLogMessage("FBO", CompLogLevelDebug, "Not power of two: %dx%d", width, height);
    }
    else
    {
      temp_size = 1;
      mipmapLevels = 1;
      while( temp_size < _width )
        temp_size *= 2, ++mipmapLevels;
      _width = temp_size;
      powOfTwoWidth = temp_size;

      temp_size = 1;
      temp_levels = 1;
      while( temp_size < _height )
        temp_size *= 2, ++temp_levels;
      _height = temp_size;
      powOfTwoHeight = temp_size;
      is_powOfTwo = true;
      if( mipmapLevels > temp_levels )
      {
        mipmapLastWidth = powOfTwoWidth / powOfTwoHeight;
        mipmapLastHeight = 1;
        mipmapLevels = temp_levels;
      }
      else
      {
        mipmapLastHeight = powOfTwoHeight / powOfTwoWidth;
        mipmapLastWidth = 1;
      }
      //  compLogMessage("FBO", CompLogLevelDebug, "Power of two size %dx%d->%dx%d; mipmaplevels: %d (lastsize: %dx%d)", width, height, _width, _height, mipmapLevels, mipmapLastWidth, mipmapLastHeight);
    }

    glGenFramebuffers(1, &fbo);
//  compLogMessage("FBO", CompLogLevelDebug, "Framebuffer generated: %d", fbo);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &lastrenderbuffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//  compLogMessage("FBO", CompLogLevelDebug, "Previously bound fbo:%d, renderbuffer:%d", last, lastrenderbuffer);

    if( stencilBuffer )
    {
      //  compLogMessage("FBO", CompLogLevelDebug, "Adding render buffer as combinded depth stencil buffer");
      glGenRenderbuffers(1, &this->depthBuffer);
      glBindRenderbuffer(GL_RENDERBUFFER, this->depthBuffer);
      if( !multisample )
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, _width,
                              _height);
      else
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample,
                                         GL_DEPTH_STENCIL, _width, _height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   GL_RENDERBUFFER, this->depthBuffer);
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                   GL_RENDERBUFFER, this->depthBuffer);
    }
    else if( depthBuffer )
    {
      //  compLogMessage("FBO", CompLogLevelDebug, "Adding texture as depth buffer");
      glGenTextures(1, &this->depthBuffer);
      if( !multisample )
      {
        glBindTexture(GL_TEXTURE_2D, this->depthBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, _width, _height, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, depthFilter);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, depthFilter);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, this->depthBuffer, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
      }
      else
      {
#if GL_ARB_texture_multisample && !defined(NOMULTISAMPLING)
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->depthBuffer);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample,
                                GL_DEPTH_COMPONENT, _width, _height, true);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D_MULTISAMPLE, this->depthBuffer, 0);
#endif
      }
    }
    else
      this->depthBuffer = 0;

    colorBuffers.resize(numColorBuffers, 0);
    glGenTextures(numColorBuffers, &colorBuffers[0]);
    for( i = 0; i < numColorBuffers; ++i )
    {
      //  compLogMessage("FBO", CompLogLevelDebug, "Adding color attachment %d",i);
      if( !multisample )
      {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, colorInternalFormat, _width, _height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, colorFilter);
        if( supportMipMaps && colorFilter == GL_LINEAR )
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR_MIPMAP_LINEAR);
        else
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, colorFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        if( supportMipMaps && colorFilter == GL_LINEAR )
          glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, colorBuffers[i], 0);
      }
      else
      {
#if GL_ARB_texture_multisample && !defined(NOMULTISAMPLING)
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorBuffers[i]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample,
                                colorInternalFormat, _width, _height, true);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D_MULTISAMPLE, colorBuffers[i], 0);
#endif
        /*glGenRenderbuffers(1,&colorBuffers[i]);
         glBindRenderbuffer(GL_RENDERBUFFER, colorBuffers[i]);
         glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample, colorInternalFormat, _width, _height);
         glBindRenderbuffer(GL_RENDERBUFFER, 0);
         glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, GL_RENDERBUFFER, colorBuffers[i]);*/
      }
    }

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, last);
    glBindRenderbuffer(GL_RENDERBUFFER, lastrenderbuffer);
    if( status != GL_FRAMEBUFFER_COMPLETE )
      throw std::runtime_error("Frame Buffer setup incomplete");
    glPopAttrib();
  }
  void FBO::resize(unsigned int width, unsigned int height)
  {
    if( !is_init )
      return;

    if( is_powOfTwo )
    {
      unsigned int temp_size = 2;
      while( temp_size < width )
        temp_size *= 2;
      width = temp_size;
      powOfTwoWidth = temp_size;

      temp_size = 2;
      while( temp_size < height )
        temp_size *= 2;
      height = temp_size;
      powOfTwoHeight = temp_size;
    }
    if( width == getTexWidth() && height == getTexHeight() )
    {
      //compLogMessage ("FBO", CompLogLevelInfo, "resize: no need to resize, equal size");
      return;
    }
    cleanUp();
    init(width, height, _colorInternalFormat, colorBuffers.size(),
         _hasDepthBuffer, _colorFilter, _depthFilter, is_powOfTwo,
         _hasStencilBuffer, _multisample);
  }
  void FBO::cleanUp()
  {
    if( !is_init )
      return;
    if( depthBuffer != 0 )
      glDeleteTextures(1, &depthBuffer);
    glDeleteTextures(colorBuffers.size(), &colorBuffers[0]);
    glDeleteFramebuffersEXT(1, &fbo);
    is_init = false;
  }
  FBO::~FBO()
  {
    cleanUp();
  }
  void FBO::bind()
  {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_to_restore);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if( _multisample )
      glEnable(GL_MULTISAMPLE);
  }
  void FBO::unbind(bool restore)
  {
    if( _multisample )
      glDisable(GL_MULTISAMPLE);
    if( restore )
      glBindFramebuffer(GL_FRAMEBUFFER, _to_restore);
    else
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  void FBO::draw(int width,
                 int height,
                 int posx,
                 int posy,
                 unsigned int colorBuffer,
                 bool resetviewport,
                 bool resetmatrix,
                 bool invertTexCoordsY)
  {
    std::vector<float> draw;
    std::vector<float> vertexcoords;

    //reset transformations and draw screen sized quad
    if( resetmatrix )
      pushMatrices();
    if( resetviewport )
    {
      glPushAttrib(GL_VIEWPORT_BIT);
      glViewport(posx, posy, width, height);
    }

    calculateCoords(vertexcoords, draw, width, height, posx, posy,
                    resetviewport, resetmatrix, invertTexCoordsY);

    glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT);
    glEnable(GL_TEXTURE_2D);
    bindTex(colorBuffer);

    renderQuad(vertexcoords, draw);

    glPopAttrib();

    if( resetviewport )
      glPopAttrib();

    if( resetmatrix )
      popMatrices();
  }
  void FBO::swapColorAttachment(int attachment)
  {
    glPushAttrib(GL_CURRENT_BIT);
    GLint last;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    //bind all
    if( attachment == -1 )
    {
      for( size_t i = 0; i < colorBuffers.size(); ++i )
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, colorBuffers[i], 0);
      return;
    }
    if( attachment >= colorBuffers.size() )
    {
      std::cerr
          << "dktSwapColorAttachment - color attachment id out of range, using attachment 0"
          << std::endl;
      attachment = 0;
    }
    for( int i = 0; i < colorBuffers.size(); ++i )
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                             GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           colorBuffers[attachment], 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, last);
    if( status != GL_FRAMEBUFFER_COMPLETE )
      throw std::runtime_error("Frame Buffer setup incomplete");
    glPopAttrib();
  }
  void FBO::swapColorAttachment(std::vector<int> attachments)
  {
    glPushAttrib(GL_CURRENT_BIT);
    GLint last;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    for( int i = 0; i < colorBuffers.size(); ++i )
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                             GL_TEXTURE_2D, 0, 0);
    for( int i = 0; i < attachments.size(); ++i )
    {
      if( attachments[i] < 0 || attachments[i] >= colorBuffers.size() )
      {
        std::cerr << "dktSwapColorAttachment - color attachment id ("
                  << attachments[i] << ") out of range, skipping" << std::endl;
        continue;
      }
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                             GL_TEXTURE_2D, colorBuffers[attachments[i]], 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, last);
    if( status != GL_FRAMEBUFFER_COMPLETE )
      throw std::runtime_error("Frame Buffer setup incomplete");
    glPopAttrib();
  }

  void FBO::activateDrawBuffer(int num, int start)
  {
    std::vector<GLenum> attachments;
    for( int i = start; i < start + num; ++i )
      attachments.push_back(GL_COLOR_ATTACHMENT0_EXT + i);
    glDrawBuffers(num, &attachments[0]);
  }

  void FBO::bindTex(int i)
  {
    GLenum target = GL_TEXTURE_2D;
    if( _multisample )
      target = GL_TEXTURE_2D_MULTISAMPLE;

    if( i < 0 )
      glBindTexture(target, 0);
    else
    {
      if( i >= colorBuffers.size() )
      {
        std::cerr
            << "dktDrawFramebufferX - texture id out of range, using texture 0"
            << std::endl;
        i = 0;
      }
      glBindTexture(target, colorBuffers[i]);
    }
  }

  void FBO::calculateCoords(std::vector<float>& vertex,
                            std::vector<float>& texcoords,
                            int width,
                            int height,
                            int posx,
                            int posy,
                            bool resetviewport,
                            bool resetmatrix,
                            bool invertTexCoords)
  {
    vertex.resize(4);
    texcoords.resize(4);

    if( resetviewport )
    {
      vertex[0] = vertex[2] = -1.0f;
      vertex[1] = vertex[3] = 1.0f;
    }
    else
    {
//     std::cout <<  "calc: posx: " << posx << " width: " << width << " posy: " << posy << " height: " << height << std::endl;
      //use given coords
      vertex[0] = posx;
      vertex[1] = posx + width;
      vertex[2] = posy;
      vertex[3] = posy + height;
    }

    if( invertTexCoords )
    {
      posy = this->height - posy - height;
      float temp = vertex[2];
      vertex[2] = vertex[3];
      vertex[3] = temp;
    }

    texcoords[0] = (float) (posx) / (float) getTexWidth();
    texcoords[1] = (float) (posx + width) / (float) getTexWidth();
    texcoords[2] = (float) (posy) / (float) getTexHeight();
    texcoords[3] = (float) (posy + height) / (float) getTexHeight();

  }

  void FBO::addColorBuffer(int num)
  {
    if( !is_init || num <= 0 )
      return;

    glPushAttrib(GL_CURRENT_BIT);

    GLint last;
    GLint lastrenderbuffer;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &lastrenderbuffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    std::vector<GLuint> newColorBuffers(num, 0);
    glGenTextures(num, &newColorBuffers[0]);
    colorBuffers.insert(colorBuffers.end(), newColorBuffers.begin(),
                        newColorBuffers.end());

    for( int i = 0; i < num; ++i )
    {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, newColorBuffers[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, _colorInternalFormat, getTexWidth(),
                   getTexHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _colorFilter);
      if( is_powOfTwo && _colorFilter == GL_LINEAR )
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
      else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _colorFilter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      if( is_powOfTwo && _colorFilter == GL_LINEAR )
        glGenerateMipmap(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, 0);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                             GL_TEXTURE_2D, newColorBuffers[i], 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, last);
    glBindRenderbuffer(GL_RENDERBUFFER, lastrenderbuffer);
    if( status != GL_FRAMEBUFFER_COMPLETE )
      throw std::runtime_error(
          "Add colorBuffer: Frame Buffer setup incomplete");
    glPopAttrib();
  }
  void FBO::renderQuad(const std::vector<float>& vertex,
                       const std::vector<float>& texcoords)
  {
//   std::cout << "Render Quad" << std::endl;
//   std::cout << "vertices: " << vertex[0] << "-" << vertex[1] << " " << vertex[2] << "-" << vertex[3] << std::endl;
//   std::cout << "texcoords: " << texcoords[0] << "-" << texcoords[1] << " " << texcoords[2] << "-" << texcoords[3] << std::endl;
    glBegin(GL_QUADS);
    glTexCoord2f(texcoords[0], texcoords[2]);
    glVertex2f(vertex[0], vertex[2]);
    glTexCoord2f(texcoords[1], texcoords[2]);
    glVertex2f(vertex[1], vertex[2]);
    glTexCoord2f(texcoords[1], texcoords[3]);
    glVertex2f(vertex[1], vertex[3]);
    glTexCoord2f(texcoords[0], texcoords[3]);
    glVertex2f(vertex[0], vertex[3]);
    glEnd();
  }
  void FBO::pushMatrices()
  {
    glPushAttrib(GL_MATRIX_MODE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
  }
  void FBO::popMatrices()
  {
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
  }
} // namespace gl
