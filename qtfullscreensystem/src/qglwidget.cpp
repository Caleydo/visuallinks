#include "qglwidget.hpp"
#include "float2.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <QBitmap>
#include <QEvent>
#include <QGLShaderProgram>
#include <QPixmap>
#include <qevent.h>
#include <GL/gl.h>

#include <QApplication>
#include <QDesktopWidget>

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
    return {};
  }

  if( !program->addShaderFromSourceFile(QGLShader::Fragment, frag) )
  {
    qWarning() << "Failed to load fragment shader (" << frag << ")\n"
               << program->log();
    return {};
  }

  if( !program->link() )
  {
    qWarning() << "Failed to link shader (" << vert << "|" << frag << ")\n"
               << program->log();
    return {};
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
    return {};

  line_borders_t ret;
  float half_width = width / 2.f;

  float2 prev_dir = (*p1 - *p0).normalize(),
         prev_normal = { prev_dir.y, -prev_dir.x };

  for( ; p1 != end; ++p0, ++p1 )
  {
    float2 dir = (*p1 - *p0).normalize(),
           mean_dir = 0.5f * (prev_dir + dir),
           normal = { mean_dir.y, -mean_dir.x };

    // project on normal
    float2 offset = normal / normal.dot(prev_normal / half_width);

    ret.first.push_back(  *p0 + offset );
    ret.second.push_back( *p0 - offset );

    prev_dir = dir;
    prev_normal = { dir.y, -dir.x };
  }

  // last point
  float2 offset = prev_normal * half_width;
  ret.first.push_back(  *p0 + offset );
  ret.second.push_back( *p0 - offset );

  return ret;
}

//------------------------------------------------------------------------------
GLWidget::GLWidget(QWidget *parent):
  QGLWidget( QGLFormat(QGL::HasOverlay | QGL::AlphaChannel), parent ),
  _render_mask( false )
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

//----------------------------------------------------------------------------
void GLWidget::setRenderMask()
{
  _render_mask = true;
}

//----------------------------------------------------------------------------
void GLWidget::clearRenderMask()
{
  _render_mask = false;
}


ShaderPtr shader;
//------------------------------------------------------------------------------
void GLWidget::initializeGL()
{
  LOG_ENTER_FUNC();

  glClearColor(0, 0, 0, 0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//------------------------------------------------------------------------------
void GLWidget::paintGL()
{
  LOG_ENTER_FUNC() << "render_mask=" << _render_mask;

  static bool took = false;
  static QPixmap bla;

//  if( !took )
//  {
//    bla = QPixmap::grabWindow(QApplication::desktop()->winId());
//    bla.save("screen.png");
//    took = true;
//  }

  if( !shader && !_render_mask )
    shader = loadShader("render_below.vert", "simple.frag");

  static int count = 0;
//  if( count++ > 1 )
//    return;

  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  GLuint tex;

  if( !_render_mask )
    bindTexture(bla.toImage());

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

  glClear(GL_COLOR_BUFFER_BIT);
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
    {  0.1,     0.25    },
    {  0.25,    0.1     },
    {  0.4,     0.05    },
    {  0.41,    0.055   },
    {  0.42,    0.06    },
    {  0.45,    0.5     },
    {  0.55,    0.6     },
    {  0.65,    0.4     },
    {  0.75,    0.5     },
    {  1.05,    0.6     },
    {  1.25,    0.55    },
    {  1.25,    0.75    },
    {  1.3,     0.75    },
    {  1.4,     0.9     },
    {  0.85,    0.8     },
    {  0.8,     0.9     },
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

    for( const float2& p: points )
      glVertex2f(p.x, p.y);

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

  glColor4f(0.5, 0.5, 0.9, 0.8);
  glBegin(GL_TRIANGLE_STRIP);
    for( const float2& p: render_points )
    {
      //glTexCoord2f(p.x/1.6, p.y);
      glVertex2f(p.x, p.y);
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

//  bla = QPixmap::grabWindow(QApplication::desktop()->winId());
//  bla.save("screen-after.png");
}

//------------------------------------------------------------------------------
void GLWidget::resizeGL(int w, int h)
{
  LOG_ENTER_FUNC() << "(" << w << "|" << h << ")" << static_cast<float>(w)/h << ":" << 1;

  glViewport(0,0,w,h);
  glOrtho(0, static_cast<float>(w)/h, 0, 1, -1.0, 1.0);
}

} // namespace qtfullscreensystem
