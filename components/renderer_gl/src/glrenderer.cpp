#include "glrenderer.h"
#include "NodeRenderer.hpp"
#include "log.hpp"

#include <GL/glu.h>
#include <iostream>

std::ostream& operator<<(std::ostream& s, const std::vector<float2>& verts)
{
  s << "[";
  for(size_t i = 0; i < verts.size(); ++i)
  {
    if( i )
      s << ", ";
    s << verts[i];
  }
  s << "]";
  return s;
}

namespace LinksRouting
{
  //----------------------------------------------------------------------------
  GlRenderer::GlRenderer():
    Configurable("GLRenderer"),
    _margin_left(0),
    _blur_x_shader(nullptr),
    _blur_y_shader(nullptr)
  {
    registerArg("NumBlur", _num_blur = 1);
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

    _slot_xray = slots.create<SlotType::Image>("/rendered-xray");
    _slot_xray->_data->type = SlotType::Image::OpenGLTexture;
  }

  //----------------------------------------------------------------------------
  void GlRenderer::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LinkDescription::LinkList>("/links");
    _subscribe_popups =
      slot_subscriber.getSlot<SlotType::TextPopup>("/popups");
    _subscribe_xray =
      slot_subscriber.getSlot<SlotType::XRayPopup>("/xray");
    _subscribe_outlines =
      slot_subscriber.getSlot<SlotType::CoveredOutline>("/covered-outlines");
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

    int w = vp[2],
        h = vp[3];
    const int upscale = 2;

    _links_fbo.init( upscale * w,
                     upscale * h,
                     GL_RGBA8, 2, false,
                     GL_LINEAR,
                     GL_NEAREST,
                     false,
                     true ); // stencil
    *_slot_links->_data =
      SlotType::Image( _links_fbo.width,
                       _links_fbo.height,
                       _links_fbo.colorBuffers.at(0) );

    _xray_fbo.init(w, h, GL_RGBA8, 1, false, GL_NEAREST);
    *_slot_xray->_data = SlotType::Image(w, h, _xray_fbo.colorBuffers.at(0));

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
  uint32_t GlRenderer::process(unsigned int type)
  {
    assert( _blur_x_shader );
    assert( _blur_y_shader );

    _slot_links->setValid(false);
    _links_fbo.bind();

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if( _subscribe_links->_data->empty() )
    {
      _links_fbo.unbind();
      return 0;
    }

    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, _links_fbo.width, _links_fbo.height);

    bool rendered_anything = false;
    if( !_subscribe_outlines->_data->popups.empty() )
    {
      rendered_anything = true;
      NodeRenderer renderer;

      for( auto const& outline: _subscribe_outlines->_data->popups )
      {
//        renderRect( outline.region_outline,
//                    2,
//                    0,
//                    Color(0,0,0,0),
//                    0.4 * _colors.front() );
        Rect reg_title = outline.region_title;
        if( !outline.preview->isVisible() )
          reg_title.size.x = std::min(150.f, reg_title.size.x);

        float fac = outline.preview->isVisible()
                  ? 1
                  : 0.5;

        renderer.renderRect(reg_title, 0, 0, fac * _colors.front());
      }
    }

    glEnable(GL_STENCIL_TEST);
    for(int pass = 0; pass <= 1; ++pass)
      rendered_anything |= renderLinks(*_subscribe_links->_data, pass);
    glDisable(GL_STENCIL_TEST);

    if( !_subscribe_popups->_data->popups.empty() )
    {
      rendered_anything = true;
      NodeRenderer renderer;

      for( auto const& popup: _subscribe_popups->_data->popups )
      {
        if( !popup.region.isVisible() )
          continue;

        renderer.renderRect(popup.region.region, popup.region.border);
        rendered_anything = true;
      }
    }

    glPopAttrib();

    _links_fbo.unbind();

    if( !rendered_anything )
      return 0;

    glDisable(GL_BLEND);
    glColor4f(1,1,1,1);

    blur(_links_fbo);
    _slot_links->setValid(true);

    // Now the xray previews
    _slot_xray->setValid(false);
    _xray_fbo.bind();

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    uint32_t flags = 0;
    for(auto const& preview: _subscribe_xray->_data->popups)
    {
      if( !preview.isVisible() )
        continue;

      HierarchicTileMapPtr tile_map = preview.tile_map.lock();
      if( !tile_map )
      {
        LOG_WARN("Preview tile_map expired!");
        continue;
      }

      flags |= MASK_DIRTY;

//      renderRect( preview.preview_region );
      tile_map->render( preview.source_region,
                        preview.source_region.size,
                        preview.preview_region,
                        -1,
                        false,
                        preview.getAlpha() );
    }

    _xray_fbo.unbind();
    _slot_xray->setValid(true);

    return flags;
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

    _colors.push_back( Color( color[0] / 256.f,
                              color[1] / 256.f,
                              color[2] / 256.f,
                              0.7f ) );
    //std::cout << "GlRenderer: Added color (" << val << ")" << std::endl;

    return true;
  }

  //----------------------------------------------------------------------------
  Color GlRenderer::getCurrentColor() const
  {
    GLfloat cur_color[4];
    glGetFloatv(GL_CURRENT_COLOR, cur_color);
    return Color(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
  }

  //----------------------------------------------------------------------------
  void GlRenderer::blur(gl::FBO& fbo)
  {
    assert(fbo.colorBuffers.size() >= 2);

    for( int i = 0; i < _num_blur; ++i )
    {
      fbo.swapColorAttachment(1);
      fbo.bind();
      fbo.bindTex(0);
      _blur_x_shader->begin();
      _blur_x_shader->setUniform1i("inputTex", 0);
      _blur_x_shader->setUniform1f("scale", 1);
      fbo.draw(fbo.width, fbo.height, 0, 0, 0, true, true);
      _blur_x_shader->end();
      fbo.unbind();

      fbo.swapColorAttachment(0);
      fbo.bind();
      fbo.bindTex(1);
      _blur_y_shader->begin();
      _blur_y_shader->setUniform1i("inputTex", 0);
      _blur_y_shader->setUniform1f("scale", 1);
#ifdef USE_DESKTOP_BLEND
      _blur_y_shader->setUniform1i("normalize_color", 0);
#else
      _blur_y_shader->setUniform1i("normalize_color", 1);
#endif
      fbo.draw(fbo.width, fbo.height, 0, 0, 1, true, true);
      _blur_y_shader->end();
      fbo.unbind();
    }
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::renderLinks( const LinkDescription::LinkList& links,
                                int pass )
  {
    bool rendered_anything = false;
    _color_cur = Color(1.0, 0.2, 0.2);

    NodeRenderer renderer;
    renderer.setUseStencil(true);
    renderer.setLineWidth(3);

    for(auto link = links.begin(); link != links.end(); ++link)
    {
      if( !_colors.empty() )
        _color_cur = _colors[ link->_color_id % _colors.size() ];
      _color_covered_cur = 0.4f * _color_cur;

      renderer.setColor(_color_cur, _color_covered_cur);

      HyperEdgeQueue hedges_open;
      HyperEdgeSet   hedges_done;

      hedges_open.push(link->_link.get());
      do
      {
        const LinkDescription::HyperEdge* hedge = hedges_open.front();
        hedges_open.pop();

        if( hedges_done.find(hedge) != hedges_done.end() )
          continue;

        if( hedge->get<bool>("covered") || hedge->get<bool>("outside") )
          glColor4fv(_color_covered_cur);
        else
          glColor4fv(_color_cur);

        auto fork = hedge->getHyperEdgeDescription();
        if( !fork )
        {
          rendered_anything |=
           renderer.renderNodes( hedge->getNodes(),
                                 &hedges_open,
                                 &hedges_done,
                                 false,
                                 pass );
        }
        else
        {
          // Don't draw where region highlights already have been drawn
          glStencilFunc(GL_EQUAL, 0, 1);
          glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

          LinkDescription::nodes_t nodes;
          for( auto& segment: fork->outgoing )
          {
            if(     pass == 1
                && !segment.trail.empty()
                /*&& segment.trail.front().x >= 0
                && segment.trail.front().y >= 24*/ )
            {
              // Draw path
              float widen_size = 0.f;
              if(   !segment.nodes.empty()
                  && segment.nodes.back()->getChildren().empty()
                  && segment.get<bool>("widen-end", true) )
              {
                if( !segment.nodes.back()
                            ->get<std::string>("virtual-outside").empty() )
                  widen_size = 13;
                else
                  widen_size = 55;
              }
              line_borders_t region = calcLineBorders( segment.trail,
                                                       3,
                                                       false,
                                                       widen_size );

              glColor4fv(   segment.get<bool>("covered")
                          ? _color_covered_cur
                          : _color_cur );
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

            // Collect nodes for drawing them after the links to prevent links
            // crossing regions.
            nodes.insert( nodes.end(),
                          segment.nodes.begin(),
                          segment.nodes.end() );
          }

          rendered_anything |= renderer.renderNodes( nodes,
                                                     &hedges_open,
                                                     &hedges_done,
                                                     false,
                                                     pass );
        }

        hedges_done.insert(hedge);
      } while( !hedges_open.empty() );
    }

    return rendered_anything;
  }

}
