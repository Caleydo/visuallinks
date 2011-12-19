#include "glrenderer.h"
#include "float2.hpp"
#include "slotdata/image.hpp"

#include <iostream>

namespace LinksRouting
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

  //----------------------------------------------------------------------------
  GlRenderer::GlRenderer() : myname("GlRenderer")
  {
    //DUMMYTEST
    registerArg("TestDouble", TestDouble);
    registerArg("TestString", TestString);
  }

  //----------------------------------------------------------------------------
  GlRenderer::~GlRenderer()
  {

  }

  //----------------------------------------------------------------------------
  void GlRenderer::publishSlots(SlotCollector& slots)
  {
    _slot_links = slots.create<SlotType::Image>("/rendered-links");
    _slot_links->_data->type = SlotType::Image::OpenGLTexture;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::startup(Core* core, unsigned int type)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  void GlRenderer::init()
  {

  }

  //----------------------------------------------------------------------------
  void GlRenderer::initGL()
  {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    _links_fbo.init(vp[2], vp[3], GL_RGB8);
    _slot_links->_data->id = _links_fbo.colorBuffers.at(0);
  }

  //----------------------------------------------------------------------------
  void GlRenderer::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  void GlRenderer::process(Type type)
  {
    //DUMMYTEST
    std::cout << "TestDouble: " << TestDouble << " TestString: " << TestString << std::endl;

    _slot_links->setValid(false);
    _links_fbo.bind();
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(2);
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
      float2(  0.0,     0.8     ),
      float2(  0.5,     1.3     )
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
      for(auto p = std::begin(render_points); p != std::end(render_points); ++p)
        glVertex2f(p->x, p->y);
    glEnd();

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

    _links_fbo.unbind();
    _slot_links->setValid(true);
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::addLinkHierarchy(LinkDescription::Node* node)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::addLinkHierarchy(LinkDescription::HyperEdge* hyperedge)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::removeLinkHierarchy(LinkDescription::Node* node)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge)
  {
    return true;
  }
};
