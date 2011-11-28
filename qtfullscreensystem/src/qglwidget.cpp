#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gl/glew.h>
#include <gl/gl.h>
#endif

#include "qglwidget.hpp"
#include "float2.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>

//#include <QEvent>
//#include <qevent.h>
#include <GL/gl.h>


#include <QApplication>
#include <QBitmap>
#include <QDesktopWidget>
//#include <QPixmap>
#include <QGLShaderProgram>

#define LOG_ENTER_FUNC() qDebug() << __FUNCTION__

namespace qtfullscreensystem
{

// internal helper...
void smoothIteration( const std::vector<float2> in,
                      std::vector<float2>& out,
                      float smoothing_factor )
{
  auto begin = in.begin(),
       end = in.end();

  decltype(begin) p0 = begin,
                  p1 = p0 + 1,
                  p2 = p1 + 1;

  float f_other = 0.5f*smoothing_factor;
  float f_this = 1.0f - smoothing_factor;

  out.push_back(*p0);

  for(; p2 != end; ++p0, ++p1, ++p2)
    out.push_back(f_other*(*p0 + *p2) + f_this*(*p1));

  out.push_back(*p1);
}

/**
 * Smooth a line given by a list of points. The more iterations you choose the
 * smoother the curve will become. How smooth it can get depends on the number
 * of input points, as no new points will be added.
 *
 * @param points
 * @param smoothing_factor
 * @param iterations
 */
template<typename Collection>
std::vector<float2> smooth( const Collection& points,
                            float smoothing_factor,
                            unsigned int iterations )

{
  // now it's time to use a specific container...
  std::vector<float2> in(std::begin(points), std::end(points));

  if( in.size() < 3 )
    return in;

  std::vector<float2> out;
  out.reserve(in.size());

  smoothIteration( in, out, smoothing_factor );

  for(unsigned int step = 1; step < iterations; ++step)
  {
    in.swap(out);
    out.clear();
    smoothIteration(in, out, smoothing_factor);
  }

  return out;
}

typedef QGLShaderProgram Shader;
typedef std::shared_ptr<Shader> ShaderPtr;

/**
 *
 * @param vert  Path to vertex shader
 * @param frag  Path to fragment shader
 */
ShaderPtr loadShader( QString vert, QString frag )
{
  qDebug() << "loadShader("<< vert << "|" << frag << ")";

  ShaderPtr program = std::make_shared<Shader>();

  if( !program->addShaderFromSourceFile(QGLShader::Vertex, vert) )
  {
    qWarning() << "Failed to load vertex shader (" << vert << ")\n"
               << program->log();
    return ShaderPtr();
  }

  if( !program->addShaderFromSourceFile(QGLShader::Fragment, frag) )
  {
    qWarning() << "Failed to load fragment shader (" << frag << ")\n"
               << program->log();
    return ShaderPtr();
  }

  if( !program->link() )
  {
    qWarning() << "Failed to link shader (" << vert << "|" << frag << ")\n"
               << program->log();
    return ShaderPtr();
  }

  return program;
}

typedef std::pair<std::vector<float2>, std::vector<float2>> line_borders_t;

/**
 * Calculate the two borders of a line which gets extruded to the given width
 *
 * @param points
 * @param width TODO which unit should be used?
 */
template<typename Collection>
line_borders_t calcLineBorders(const Collection& points, float width)
{
  auto begin = std::begin(points),
       end = std::end(points);

  decltype(begin) p0 = begin,
                  p1 = p0 + 1;

  if( p0 == end || p1 == end )
    return line_borders_t();

  line_borders_t ret;
  float half_width = width / 2.f;

  float2 prev_dir = (*p1 - *p0).normalize(),
         prev_normal = float2( prev_dir.y, -prev_dir.x );

  for( ; p1 != end; ++p0, ++p1 )
  {
    float2 dir = (*p1 - *p0).normalize(),
           mean_dir = 0.5f * (prev_dir + dir),
           normal = float2( mean_dir.y, -mean_dir.x );

    // project on normal
    float2 offset = normal / normal.dot(prev_normal / half_width);

    ret.first.push_back(  *p0 + offset );
    ret.second.push_back( *p0 - offset );

    prev_dir = dir;
    prev_normal = float2( dir.y, -dir.x );
  }

  // last point
  float2 offset = prev_normal * half_width;
  ret.first.push_back(  *p0 + offset );
  ret.second.push_back( *p0 - offset );

  return ret;
}

//------------------------------------------------------------------------------
GLWidget::GLWidget(QWidget *parent):
  QGLWidget( parent )
{
  if( !isValid() )
    qFatal("Unable to create OpenGL context (not valid)");

  qDebug
  (
    "Created GLWidget with OpenGL %d.%d",
    format().majorVersion(),
    format().minorVersion()
  );
}

//------------------------------------------------------------------------------
GLWidget::~GLWidget()
{

}

ShaderPtr shader;
//------------------------------------------------------------------------------
void GLWidget::initializeGL()
{
  LOG_ENTER_FUNC();

#if WIN32
  if(glewInit() != GLEW_OK)
    qFatal("Unable to init Glew");
#endif

  if( _fbo_links || _fbo_desktop )
    qDebug("At least one fbo already initialized!");

  _fbo_links = std::make_shared<QGLFramebufferObject>
    (
      size(),
      QGLFramebufferObject::Depth
    );
  _fbo_desktop = std::make_shared<QGLFramebufferObject>
     (
       size(),
       QGLFramebufferObject::Depth
     );

  if( !_fbo_links->isValid() || !_fbo_desktop->isValid() )
    qFatal("Failed to create framebufferobjects!");

  glClearColor(0, 0, 0, 0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//------------------------------------------------------------------------------
void GLWidget::paintGL()
{
  LOG_ENTER_FUNC();

  // render links to framebuffer
  _fbo_links->bind();
  glClear(GL_COLOR_BUFFER_BIT);

//  if( !shader && _render_mode != RENDER_MASK )
//    shader = loadShader("render_below.vert", "simple.frag");

//  glActiveTexture(GL_TEXTURE0);
//  GLuint tex = -1;
//
//  if( _render_mode == RENDER_MASK )
//  {
//    glDisable(GL_TEXTURE_2D);
//  }
//  else
//  {
//    glEnable(GL_TEXTURE_2D);
//
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
//
//    tex = bindTexture(bla.toImage());
//  }
#if 0
  if( shader && false )
  {
    shader->setUniformValue("desktop", tex);
    shader->bind();

//    glActiveTexture(GL_TEXTURE0);
//    glEnable(GL_TEXTURE_2D);
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

//    shader->setUniformValue("fromInput", 0);
    shader->setUniformValue("desktop", tex);
  //        renderBelowShader->setUniform1i("textinfo", 2);
  //        renderBelowShader->setUniform1i("background", 3);
  //        renderBelowShader->setUniform1i("foreground", 4);
//    shader->setUniformValue("gridTexStep", 3.2f/1680, 2.f/1050);
//    shader->setUniformValue("distanceThreshold", 10);
    shader->setUniformValue("inv_image_dimensions", 1.f/1.6, 1.f/1);
//    shader->setUniformValue("fade",0.5f);
//    shader->setUniformValue("maxalpha",0.5f);
//    shader->setUniformValue("belowalpha", 0.3f);
  }
#endif
  glColor3f(1.0, 1.0, 1.0);

  glLineWidth(2);
#if 0
  glColor3f(0, 1, 0);
  glBegin(GL_LINES);
  glVertex2f(0, 0);
  glVertex2f(0.9, 0.9);
  glEnd();

  glColor3f(1, 1, 0);
  glBegin(GL_LINES);
  glVertex2f(0, 0);
  glVertex2f(-1.5, 0.9);
  glEnd();

  glColor3f(0, 0, 1);
  glBegin(GL_LINES);
  glVertex2f(0, 0);
  glVertex2f(0.9, -0.9);
  glEnd();

  glColor3f(1, 0, 0);
  glBegin(GL_LINES);
  glVertex2f(-1, -1);
  glVertex2f(1, 1);
  glEnd();
#endif
  float2 points[] = {
    float2( -0.7,    -0.5     ),
    float2( -0.55,   -0.8     ),
    float2( -0.4,    -0.9     ),
    float2( -0.39,   -0.91    ),
    float2( -0.38,   -0.92    ),
    float2( -0.35,    0.0     ),
    float2( -0.25,    0.2     ),
    float2( -0.15,   -0.2     ),
    float2( -0.05,    0.0     ),
    float2(  0.25,    0.2     ),
    float2(  0.65,    0.1     ),
    float2(  0.65,    0.5     ),
    float2(  0.5,     0.5     ),
    float2(  0.6,     0.8     ),
    float2(  0.05,    0.6     ),
    float2(  0.0,     0.8     )
  };

  std::vector<float2> smoothed_points = smooth(points, 0.1, 2);
  line_borders_t line_borders = calcLineBorders(smoothed_points, 0.015);

  std::vector<float2> render_points;
  render_points.reserve(line_borders.first.size() * 2);

  for( auto first = std::begin(line_borders.first),
            second = std::begin(line_borders.second);
       first != std::end(line_borders.first);
       ++first,
       ++second )
  {
    render_points.push_back(*first);
    render_points.push_back(*second);
  }

  glColor3f(1, 0.8, 0.8);
  glBegin(GL_LINE_STRIP);

  //for( const float2& p: points )
  //  glVertex2f(p.x, p.y);
  for(auto p = std::begin(points); p != std::end(points); ++p)
    glVertex2f(p->x, p->y);

  glEnd();

  glLineWidth(1);
#if 0
  glColor3f(1, 0, 1);
  glBegin(GL_LINE_STRIP);
    for( const float2& p: line_borders.first )
    {
      glVertex2f(p.x, p.y);
    }
  glEnd();

  glBegin(GL_LINE_STRIP);
    for( const float2& p: line_borders.second )
      glVertex2f(p.x, p.y);
  glEnd();
#endif

  glColor3f(0.5, 0.5, 0.9);
  glBegin(GL_TRIANGLE_STRIP);
    //for( const float2& p: render_points )
    //{
    //  //glTexCoord2f(p.x/1.6, p.y);
    //  glVertex2f(p.x, p.y);
    //}
    for(auto p = std::begin(render_points); p != std::end(render_points); ++p)
    {
      //glTexCoord2f(p->x/1.6, p->y);
      glVertex2f(p->x, p->y);
    }
  glEnd();

  glFinish();


#if 0
  glColor3f(1, 0, 1);
  glBegin(GL_LINE_STRIP);

    for( const float2& p: smooth(points, 0.4,1) )
      glVertex2f(p.x, p.y);
  glEnd();

  glColor3f(1, 0.5, 1);
  glBegin(GL_LINE_STRIP);

  for( const float2& p: smooth(points, 0.4,2) )
    glVertex2f(p.x, p.y);

//  Point p1(-5, 1),
//        p2(-0.95, 1),
//        p3(0.5, -0.8),
//        p4(0.5, -5);
//
//
//  for( float t = 0; t <= 1; t += 0.025 )
//  {
//    Point p = 0.5 * ( ( -p1 + 3*p2 - 3*p3 + p4) * t*t*t
//                    + (2*p1 - 5*p2 + 4*p3 - p4) * t*t
//                    + ( -p1        +   p3     ) * t
//                    +         2*p2
//                    );
//
//    glVertex2f(p.x, p.y);
//  }

  glEnd();
#endif

  _fbo_links->release();

  QImage links = _fbo_links->toImage();
  links.save("fbo.png");

  QBitmap mask =
    QBitmap::fromImage( links.createMaskFromColor( qRgba(0,0,0,0) ) );
  mask.save("mask.png");
  setMask(mask);

  // get screenshot (grab screen and remove links)
  _fbo_desktop->bind();

  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
  bindTexture( QPixmap::grabWindow(QApplication::desktop()->winId()) );

  glActiveTexture(GL_TEXTURE1);
//  glEnable(GL_TEXTURE_2D);
//  glBindTexture(GL_TEXTURE_2D, _fbo_links->texture());

  glBegin( GL_QUADS );

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
  glDisable(GL_TEXTURE_2D);

  _fbo_desktop->release();

  static int counter = 0;
  counter++;
  _fbo_desktop->toImage().save( QString("desktop%1.png").arg(counter) );
}

//------------------------------------------------------------------------------
void GLWidget::resizeGL(int w, int h)
{
  LOG_ENTER_FUNC() << "(" << w << "|" << h << ")" << static_cast<float>(w)/h << ":" << 1;

  glViewport(0,0,w,h);
  //glOrtho(0, static_cast<float>(w)/h, 0, 1, -1.0, 1.0);
}

} // namespace qtfullscreensystem
