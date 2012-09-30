#include "glrenderer.h"
#include "float2.hpp"
#include "log.hpp"

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
  line_borders_t calcLineBorders( const Collection& points,
                                  float width,
                                  bool closed = false )
  {
    auto begin = std::begin(points),
         end = std::end(points);

    decltype(begin) p0 = begin,
                     p1 = p0 + 1;

    if( p0 == end || p1 == end )
      return line_borders_t();

    line_borders_t ret;
    float w_out = 0.5 * width,
          w_in = 0.5 * width;

    float2 prev_dir = (closed ? (*p0 - *(end - 1)) : (*p1 - *p0)).normalize(),
           prev_normal = float2( prev_dir.y, -prev_dir.x );

    for( ; p0 != end; ++p0 )
    {
      float2 normal, dir;

      if( closed || p1 != end )
      {
        dir = (((closed && p1 == end) ? *begin : *p1) - *p0).normalize();
        float2 mean_dir = 0.5f * (prev_dir + dir);
        normal = float2( mean_dir.y, -mean_dir.x );
      }
      else // !closed && p1 == end
      {
        normal = float2( prev_dir.y, -prev_dir.x );
      }

      // project on normal
      float2 offset_out = normal / normal.dot(prev_normal / (w_out)),
             offset_in = normal / normal.dot(prev_normal / (w_in));

      ret.first.push_back(  *p0 + offset_out );
      ret.second.push_back( *p0 - offset_in );

      prev_dir = dir;
      prev_normal = float2( dir.y, -dir.x );

      if(p1 != end)
        ++p1;
    }

    if( closed )
    {
      ret.first.push_back( ret.first.front() );
      ret.second.push_back( ret.second.front() );
    }

    return ret;
  }

  //----------------------------------------------------------------------------
  GlRenderer::GlRenderer():
    Configurable("GLRenderer")
  {
    registerArg("enabled", _enabled = true);
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
  void GlRenderer::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LinkDescription::LinkList>("/links");
    _subscribe_popups =
      slot_subscriber.getSlot<SlotType::TextPopup>("/popups");
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
  bool GlRenderer::initGL()
  {
    static bool init = false;
    if( init )
      return true;

    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);

    _links_fbo.init(vp[2], vp[3], GL_RGBA8, 2, false, GL_NEAREST);
    *_slot_links->_data = SlotType::Image(vp[2], vp[3], _links_fbo.colorBuffers.at(0));

    _blur_x_shader = _shader_manager.loadfromFile(0, "blurX.glsl");
    _blur_y_shader = _shader_manager.loadfromFile(0, "blurY.glsl");

    init = true;
    return true;
  }

  //----------------------------------------------------------------------------
  void GlRenderer::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  void GlRenderer::process(unsigned int type)
  {
    if( !_enabled )
      return;

    assert( _blur_x_shader );
    assert( _blur_y_shader );

    _slot_links->setValid(false);
    _links_fbo.bind();

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    if( _subscribe_links->_data->empty() )
      return _links_fbo.unbind();

    bool rendered_anything = renderLinks(*_subscribe_links->_data);
    if( !_subscribe_popups->_data->popups.empty() )
    {
      rendered_anything = true;

      glColor3f(0.3, 0.3, 0.3);
      glBegin(GL_QUADS);
      for( auto popup = _subscribe_popups->_data->popups.begin();
                  popup != _subscribe_popups->_data->popups.end();
                ++popup )
      {
        if( !popup->visible )
          continue;

        glVertex2f(popup->pos.x, popup->pos.y);
        glVertex2f(popup->pos.x + popup->size.x, popup->pos.y);
        glVertex2f(popup->pos.x + popup->size.x, popup->pos.y + popup->size.y);
        glVertex2f(popup->pos.x, popup->pos.y + popup->size.y);

        rendered_anything = true;
      }
      glEnd();

      glColor3f(1, 1, 1);
      glBegin(GL_QUADS);
      for( auto popup = _subscribe_popups->_data->popups.begin();
                  popup != _subscribe_popups->_data->popups.end();
                ++popup )
      {
        if( !popup->visible )
          continue;

        glVertex2f(popup->pos.x + 2,                 popup->pos.y + 2);
        glVertex2f(popup->pos.x + popup->size.x - 2, popup->pos.y + 2);
        glVertex2f(popup->pos.x + popup->size.x - 2, popup->pos.y + popup->size.y - 2);
        glVertex2f(popup->pos.x + 2,                 popup->pos.y + popup->size.y - 2);
      }
      glEnd();
    }
    _links_fbo.unbind();

    if( !rendered_anything )
      return;

    glDisable(GL_BLEND);
    glColor4f(1,1,1,1);

    // blur
    int width = _slot_links->_data->width,
        height = _slot_links->_data->height;
for( int i = 0; i < 1; ++i )
{
    _links_fbo.swapColorAttachment(1);
    _links_fbo.bind();
    _links_fbo.bindTex(0);
    _blur_x_shader->begin();
    _blur_x_shader->setUniform1i("inputTex", 0);
    _blur_x_shader->setUniform1f("scale", 1);
    _links_fbo.draw(width, height, 0, 0, 0, true, true);
    _blur_x_shader->end();
    _links_fbo.unbind();

    _links_fbo.swapColorAttachment(0);
    _links_fbo.bind();
    _links_fbo.bindTex(1);
    _blur_y_shader->begin();
    _blur_y_shader->setUniform1i("inputTex", 0);
    _blur_y_shader->setUniform1f("scale", 1);
#ifdef USE_DESKTOP_BLEND
    _blur_y_shader->setUniform1i("normalize_color", 0);
#else
    _blur_y_shader->setUniform1i("normalize_color", 1);
#endif
    _links_fbo.draw(width, height, 0, 0, 1, true, true);
    _blur_y_shader->end();
    _links_fbo.unbind();
}
    _slot_links->setValid(true);
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::setString(const std::string& name, const std::string& val)
  {
    if( name != "link-color" )
      return ComponentArguments::setString(name, val);

    char *end;
    long int color[3];
    color[0] = strtol(val.c_str(), &end, 10);
    color[1] = strtol(end,         &end, 10);
    color[2] = strtol(end,         0,    10);

    _colors.push_back(Color(color[0], color[1], color[2]));
    std::cout << "GlRenderer: Added color (" << val << ")" << std::endl;

    return true;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::renderLinks(const LinkDescription::LinkList& links)
  {
    bool rendered_anything = false;
    glColor3f(1.0, 0.2, 0.2);

    for(auto link = links.begin(); link != links.end(); ++link)
    {
      if( !_colors.empty() )
        glColor3fv(_colors[ link->_color_id % _colors.size() ]);

      HyperEdgeQueue hedges_open;
      HyperEdgeSet   hedges_done;

      hedges_open.push(link->_link.get());
      do
      {
        const LinkDescription::HyperEdge* hedge = hedges_open.front();
        hedges_open.pop();

        if( hedges_done.find(hedge) != hedges_done.end() )
          continue;

        auto fork = hedge->getHyperEdgeDescription();
        if( !fork )
        {
          if( renderNodes(hedges_open, hedges_done, hedge->getNodes()) )
            rendered_anything = true;

          continue;
        }

        for( auto segment = fork->outgoing.begin();
             segment != fork->outgoing.end();
             ++segment )
        {
          if( renderNodes(hedges_open, hedges_done, segment->nodes) )
            rendered_anything = true;

          if( segment->trail.empty() )
            continue;

          // Draw path
          std::vector<float2> points;
          points.reserve(segment->trail.size() + 1);
          points.push_back(fork->position);
          points.insert(points.end(), segment->trail.begin(), segment->trail.end());
          points = smooth(points, 0.4, 10);
          line_borders_t region = calcLineBorders(points, 3);
          glBegin(GL_TRIANGLE_STRIP);
          for( auto first = std::begin(region.first),
                    second = std::begin(region.second);
               first != std::end(region.first);
               ++first,
               ++second )
          {
            glVertex2f(first->x, first->y);
            glVertex2f(second->x, second->y);
          }
          glEnd();
        }

        hedges_done.insert(hedge);
      } while( !hedges_open.empty() );
    }

    return rendered_anything;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::renderNodes( HyperEdgeQueue& hedges_open,
                                HyperEdgeSet& hedges_done,
                                const LinkDescription::nodes_t& nodes )
  {
    bool rendered_anything = false;

    for( auto node = nodes.begin(); node != nodes.end(); ++node )
    {
      if( (*node)->get<bool>("hidden", false) )
        continue;

      for(auto child = (*node)->getChildren().begin();
               child != (*node)->getChildren().end();
             ++child )
        hedges_open.push( child->get() );

      if( (*node)->getVertices().empty() )
        continue;

      bool filled = (*node)->get<bool>("filled", false);
      line_borders_t region = calcLineBorders((*node)->getVertices(), 3, true);
      glBegin(filled ? GL_POLYGON : GL_TRIANGLE_STRIP);
      for( auto first = std::begin(region.first),
                second = std::begin(region.second);
           first != std::end(region.first);
           ++first,
           ++second )
      {
        if( !filled )
          glVertex2f(first->x, first->y);
        glVertex2f(second->x, second->y);
      }
      glEnd();
      rendered_anything = true;
    }

    return rendered_anything;
  }

};
