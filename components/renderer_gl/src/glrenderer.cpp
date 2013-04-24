#include "glrenderer.h"
#include "float2.hpp"
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
      out.push_back(f_other * (*p0 + *p2) + f_this * (*p1));

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
                                  bool closed = false,
                                  float widen_end = 0.f )
  {
    auto begin = std::begin(points),
         end = std::end(points);

    decltype(begin) p0 = begin,
                     p1 = p0 + 1;

    if( p0 == end || p1 == end )
      return line_borders_t();

    widen_end = std::min(widen_end, (*(end - 2) - *(end - 1)).length());

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

        prev_dir = dir;
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

      prev_normal = prev_dir.normal();

      if(p1 != end)
        ++p1;
    }

    if( closed )
    {
      ret.first.push_back( ret.first.front() );
      ret.second.push_back( ret.second.front() );
    }
    else if( widen_end > 1.f )
    {
      float f = widen_end / 35.f;

      ret.first.back() -= f * 35 * prev_dir;
      ret.second.back() -= f * 35 * prev_dir;

      const float offsets[][2] = {
        {16, 1.5},
        {11, 2},
        { 8, 2.5}
      };

      for(size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i)
      {
        ret.first.push_back( ret.first.back() + f * offsets[i][0] * prev_dir
                                              + offsets[i][1] * prev_normal );
        ret.second.push_back( ret.second.back() + f * offsets[i][0] * prev_dir
                                                - offsets[i][1] * prev_normal );
      }
    }

    return ret;
  }

  //----------------------------------------------------------------------------
  GlRenderer::GlRenderer():
    Configurable("GLRenderer"),
    _partitions_src(0),
    _partitions_dest(0)
  {

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

    _xray_fbo.init(vp[2], vp[3], GL_RGBA8, 1, false, GL_NEAREST);
    *_slot_xray->_data = SlotType::Image(vp[2], vp[3], _xray_fbo.colorBuffers.at(0));

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
    assert( _blur_x_shader );
    assert( _blur_y_shader );

    _slot_links->setValid(false);
    _links_fbo.bind();

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    if( _subscribe_links->_data->empty() )
      return _links_fbo.unbind();

    _partitions_src = 0;
    _partitions_dest = 0;

    bool rendered_anything = false;
    for(int pass = 0; pass <= 1; ++pass)
      rendered_anything |= renderLinks(*_subscribe_links->_data, pass);

    if( !_subscribe_popups->_data->popups.empty() )
    {
      rendered_anything = true;

      for( auto const& popup: _subscribe_popups->_data->popups )
      {
        if( !popup.region.visible )
          continue;

        _partitions_src = 0;
        _partitions_dest = 0;

        renderRect(popup.region.region, popup.region.border);
        rendered_anything = true;

        if( !popup.hover_region.visible )
          continue;

        renderRect( popup.hover_region.region,
                    popup.hover_region.border,
                    0,
                    Color(0.3,0.3,0.3) );


        const Rect& hover = popup.hover_region.region,
                  & src = popup.hover_region.src_region,
                  & scroll = popup.hover_region.scroll_region;

        HierarchicTileMapPtr tile_map = popup.hover_region.tile_map.lock();
        renderTileMap( tile_map,
                       src,
                       hover,
                       popup.hover_region.zoom,
                       popup.auto_resize );

        // ---------
        // Scrollbar

        if( src.size.y < scroll.size.y )
        {
          float bar_height = std::max(src.size.y / scroll.size.y, 0.05f)
                           * hover.size.y;
          float rel_pos_y = src.pos.y / (scroll.size.y - src.size.y);

          float2 scroll_pos = hover.pos
                            + float2( hover.size.x + 4,
                                      rel_pos_y * (hover.size.y - bar_height) );

          glColor4f(1,0.5,0.25,1);
          glLineWidth(4);
          glBegin(GL_LINE_STRIP);
            glVertex2f(scroll_pos.x, scroll_pos.y);
            glVertex2f(scroll_pos.x, scroll_pos.y + bar_height);
          glEnd();
        }

        glColor3f(1, 0, 0);

        GLdouble proj[16];
        glGetDoublev(GL_PROJECTION_MATRIX, proj);

        float offset_x = (proj[12] + 1) / proj[0];
        float offset_y = (proj[13] + 1) / proj[5];

        glPushAttrib(GL_VIEWPORT_BIT);
        glViewport( hover.pos.x + offset_x,
                    hover.pos.y + offset_y,
                    hover.size.x,
                    hover.size.y );

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, hover.size.x, 0, hover.size.y, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        float scale = hover.size.y / src.size.y;
        glScalef(scale, scale, 0);

        float2 offset = src.pos;
        if( !popup.auto_resize && src.size.x > scroll.size.x )
          offset.x -= (src.size.x - scroll.size.x) / 2;

        glTranslatef(-offset.x, -offset.y, 0);

        const float w = 4.f;
        const float2 vp_pos = popup.hover_region.offset + 0.5f * float2(w, w);
        const float2 vp_dim = popup.hover_region.dim - 4.f * float2(w, w);

        if( tile_map )
        {
          _partitions_src = &tile_map->partitions_src;
          _partitions_dest = &tile_map->partitions_dest;
        }

        glLineWidth(w);
        glBegin(GL_LINE_LOOP);
          glVertex2f(vp_pos.x, vp_pos.y);
          glVertex2f(vp_pos.x + vp_dim.x, vp_pos.y);
          glVertex2f(vp_pos.x + vp_dim.x, vp_pos.y + vp_dim.y);
          glVertex2f(vp_pos.x, vp_pos.y + vp_dim.y);
        glEnd();

        renderNodes(popup.nodes, 3.f / scale, 0, 0, true, 1);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glPopAttrib();
      }
    }

    _links_fbo.unbind();

    if( !rendered_anything )
      return;

    glDisable(GL_BLEND);
    glColor4f(1,1,1,1);

    blur(_links_fbo);
    _slot_links->setValid(true);

    // Now the xray previews
    _slot_xray->setValid(false);
    _xray_fbo.bind();

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    for(auto const& preview: _subscribe_xray->_data->popups)
    {
      if( !preview.node->get<bool>("hover") )
        continue;

      HierarchicTileMapPtr tile_map = preview.tile_map.lock();
      if( !tile_map )
      {
        LOG_WARN("Preview tile_map expired!");
        continue;
      }

//      renderRect( preview.preview_region );
      renderTileMap( preview.tile_map.lock(),
                     preview.source_region,
                     preview.preview_region );
    }

    _xray_fbo.unbind();
    _slot_xray->setValid(true);
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
                              0.9f ) );
    std::cout << "GlRenderer: Added color (" << val << ")" << std::endl;

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

    for( int i = 0; i < 1; ++i )
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

    for(auto link = links.begin(); link != links.end(); ++link)
    {
      if( !_colors.empty() )
        _color_cur = _colors[ link->_color_id % _colors.size() ];
      _color_covered_cur = 0.4f * _color_cur;

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

        if( pass > 0 )
        {
          // First pass is used by xray previews which are given in absolute
          // coordinates.
          float2 offset = hedge->get<float2>("screen-offset");
          glPushMatrix();
          glTranslatef(offset.x, offset.y, 0);
        }

        auto fork = hedge->getHyperEdgeDescription();
        if( !fork )
        {
          if( renderNodes( hedge->getNodes(),
                           3.f,
                           &hedges_open,
                           &hedges_done,
                           false,
                           pass ) )
            rendered_anything = true;
        }
        else
          for( auto& segment: fork->outgoing )
          {
            if(     pass == 1
                && !segment.trail.empty()
                /*&& segment.trail.front().x >= 0
                && segment.trail.front().y >= 24*/ )
            {
              // Draw path
              //points = smooth(points, 0.4, 10);
              float widen_size = 0.f;
              if(    segment.nodes.back()->getChildren().empty()
                  && segment.get<bool>("widen-end", true) )
              {
                if( !segment.nodes.back()->get<std::string>("virtual-outside").empty() )
                  widen_size = 13;
                else
                  widen_size = 55;
              }
              line_borders_t region = calcLineBorders(segment.trail, 3, false, widen_size);

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

            if( renderNodes( segment.nodes,
                             3.f,
                             &hedges_open,
                             &hedges_done,
                             false,
                             pass ) )
              rendered_anything = true;
          }

        if( pass > 0 )
        {
          glPopMatrix();
        }

        hedges_done.insert(hedge);
      } while( !hedges_open.empty() );
    }

    return rendered_anything;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::renderNodes( const LinkDescription::nodes_t& nodes,
                                float line_width,
                                HyperEdgeQueue* hedges_open,
                                HyperEdgeSet* hedges_done,
                                bool render_all,
                                int pass )
  {
    bool rendered_anything = false;

    for( auto node = nodes.begin(); node != nodes.end(); ++node )
    {
      if( (*node)->get<bool>("hidden") && !render_all )
        continue;

      if( hedges_open )
      {
        for(auto child = (*node)->getChildren().begin();
                 child != (*node)->getChildren().end();
               ++child )
          hedges_open->push( child->get() );
      }

      if( (*node)->getVertices().empty() )
        continue;

      if( pass == 0 )
      {
        if(   !render_all
            && (*node)->get<bool>("hover") )
        {
          const Rect rp = (*node)->get<Rect>("covered-preview-region");
          renderRect(rp, 2.f, 0, 0.07 * _color_cur, 2 * _color_cur);
          const Rect r = (*node)->get<Rect>("covered-region");
          renderRect(r, 3.f, 0, 0.2 * _color_cur, 2 * _color_cur);
          rendered_anything = true;
        }
        continue;
      }

      Color color_cur = !render_all && ( (*node)->get<bool>("covered")
                                       || (*node)->get<bool>("outside")
                                       )
                      ? _color_covered_cur
                      : _color_cur;

      bool filled = (*node)->get<bool>("filled", false);
      if( !filled && !render_all )
      {
        Color light = 0.5 * color_cur;
        light.a *= 0.6;
        glColor4fv(light);
        glBegin(GL_POLYGON);
        for( auto vert = std::begin((*node)->getVertices());
                  vert != std::end((*node)->getVertices());
                ++vert )
          glVertex2f(vert->x, vert->y);
        glEnd();
      }
      glColor4fv(color_cur);
      line_borders_t region = calcLineBorders( (*node)->getVertices(),
                                               line_width,
                                               true );
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

  //----------------------------------------------------------------------------
  bool GlRenderer::renderRect( const Rect& rect,
                               size_t b,
                               GLuint tex,
                               const Color& fill,
                               const Color& border )
  {
    if( b )
    {
      glColor4fv(border);
      glBegin(GL_QUADS);
        glVertex2f(rect.pos.x - b,               rect.pos.y - b);
        glVertex2f(rect.pos.x + rect.size.x + b, rect.pos.y - b);
        glVertex2f(rect.pos.x + rect.size.x + b, rect.pos.y + rect.size.y + b);
        glVertex2f(rect.pos.x - b,               rect.pos.y + rect.size.y + b);
      glEnd();
    }

    if( tex )
    {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, tex);
    }

    glColor4fv(fill);
    glBegin(GL_QUADS);
      glTexCoord2f(0, 0);
      glVertex2f(rect.pos.x,               rect.pos.y);
      glTexCoord2f(1, 0);
      glVertex2f(rect.pos.x + rect.size.x, rect.pos.y);
      glTexCoord2f(1, 1);
      glVertex2f(rect.pos.x + rect.size.x, rect.pos.y + rect.size.y);
      glTexCoord2f(0, 1);
      glVertex2f(rect.pos.x,               rect.pos.y + rect.size.y);
    glEnd();

    if( tex )
    {
      glDisable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, 0);
    }

    return true;
  }

  //----------------------------------------------------------------------------
  bool GlRenderer::renderTileMap( const HierarchicTileMapPtr& tile_map,
                                  const Rect& src_region,
                                  const Rect& target_region,
                                  size_t zoom,
                                  bool auto_center )
  {
    if( !tile_map )
      return false;

    MapRect rect = tile_map->requestRect(src_region, zoom);
    float2 rect_size = rect.getSize();
    MapRect::QuadList quads = rect.getQuads();
    for(auto quad = quads.begin(); quad != quads.end(); ++quad)
    {
      if( quad->first->type == Tile::ImageRGBA8 )
      {
        std::cout << "load tile to GPU (" << quad->first->width
                                   << "x" << quad->first->height
                                   << ")" << std::endl;
        GLuint tex_id;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);

        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        glTexImage2D(
          GL_TEXTURE_2D,
          0,
          GL_RGBA,
          quad->first->width,
          quad->first->height,
          0,
          GL_RGBA,
          GL_UNSIGNED_BYTE,
          quad->first->pdata
        );

        delete[] quad->first->pdata;
        quad->first->id = tex_id;
        quad->first->type = Tile::OpenGLTexture;
      }

      if( quad->first->type != Tile::OpenGLTexture )
        continue;

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, quad->first->id);

      float offset_x = 0;
      if( !auto_center && target_region.size.x > rect_size.x )
        offset_x = (target_region.size.x - rect_size.x) / 2;

      glPushMatrix();
      glTranslatef( target_region.pos.x + offset_x,
                    target_region.pos.y,
                    0 );

      glColor4f(1,1,1,1);
      glBegin(GL_QUADS);
      for(size_t i = 0; i < quad->second._coords.size(); ++i)
      {
        glTexCoord2fv(quad->second._tex_coords[i].ptr());
        glVertex2fv(quad->second._coords[i].ptr());
      }
      glEnd();

      glPopMatrix();

      glDisable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, 0);
    }

    return true;
  }

  //----------------------------------------------------------------------------
  float2 GlRenderer::glVertex2f(float x, float y)
  {
#if 1
    if( _partitions_dest && _partitions_src )
    {
      float last_src = 0,
            last_dest = 0,
            ref_y = y;
      for( auto src = _partitions_src->begin(),
                dest = _partitions_dest->begin();
                src != _partitions_src->end() &&
                dest != _partitions_dest->end();
              ++src,
              ++dest )
      {
        if( ref_y <= src->x )
        {
          float t = (ref_y - last_src) / (src->x - last_src);
          y = (1 - t) * last_dest + t * dest->x;
        }
        else if( ref_y <= src->y )
        {
          y -= src->x - dest->x;
        }
        else
        {
          last_src = src->y;
          last_dest = dest->y;
          continue;
        }
        break;
      }
    }
#endif
    ::glVertex2f(x, y);
    return float2(x, y);
  }

}
