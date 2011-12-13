
#ifndef DKTXFBO_H
#define DKTXFBO_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include "dkt_gl.h"
#include <vector>

class dktXFbo
{
  bool is_powOfTwo;
  unsigned int powOfTwoWidth;
  unsigned int powOfTwoHeight;
  unsigned int mipmapLevels, mipmapLastWidth, mipmapLastHeight; 
  bool _restore;
  GLint _to_restore;
  
  unsigned int _colorInternalFormat , _colorFilter, _depthFilter, _multisample; 
  bool _hasDepthBuffer, _hasStencilBuffer;

  dktXFbo( const dktXFbo& b );
  const dktXFbo& operator = ( const dktXFbo& b );
  
  public:
  GLuint fbo;
  std::vector<GLuint> colorBuffers;
  GLuint depthBuffer;
  unsigned int width;
  unsigned int height;
  bool is_init;

  unsigned int getTexWidth() { return is_powOfTwo?powOfTwoWidth:width; }
  unsigned int getTexHeight() { return is_powOfTwo?powOfTwoHeight:height; }
  dktXFbo() : is_init(false) { }
  dktXFbo( unsigned int width, unsigned int height, unsigned int colorInternalFormat =  GL_RGBA8, unsigned int numColorBuffers = 1, bool depthBuffer = true, unsigned int colorFilter = GL_LINEAR, unsigned int depthFilter = GL_NEAREST, bool supportMipMaps = false, bool stencilBuffer = false, unsigned int multisample = 0);
  ~dktXFbo();
  void init ( unsigned int width, unsigned int height, unsigned int colorInternalFormat =  GL_RGBA8, unsigned int numColorBuffers = 1, bool depthBuffer = true, unsigned int colorFilter = GL_LINEAR, unsigned int depthFilter = GL_NEAREST, bool supportMipMaps = false, bool stencilBuffer = false, unsigned int multisample = 0);
  bool isInit() const { return is_init; }
  void bind();
  void unbind(bool restore = false);
  void draw(int width, int height, int posx = 0, int posy = 0, unsigned int colorBuffer = 0, bool resetviewport = false, bool resetmatrix = false, bool invertTexCoordsY = false);
  void swapColorAttachment(int attachment);
  void swapColorAttachment(std::vector<int> attachments);
  void activateAllColorAttachments() { swapColorAttachment(-1); }
  void resize(unsigned int width, unsigned int height);
  void cleanUp();
  void addColorBuffer(int num);
  void activateDrawBuffer(int num);
  void bindTex(int i);
  
  void calculateCoords(std::vector<float>& vertex, std::vector<float>& texcoords,  int width,  int height, int posx, int posy,  bool resetviewport = false, bool resetmatrix = false, bool invertTexCoords = false);
  static void renderQuad(const std::vector<float>& vertex, const std::vector<float>& texcoords);
  static void pushMatrices();
  static void popMatrices();
};

#endif