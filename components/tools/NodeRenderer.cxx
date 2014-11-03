#include "NodeRenderer.hpp"

#include <GL/gl.h>

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  NodeRenderer::NodeRenderer( Partitions *partitions_src,
                              Partitions *partitions_dest,
                              unsigned int margin_left ):
    _partitions_src(partitions_src),
    _partitions_dest(partitions_dest),
    _margin_left(margin_left),
    _use_stencil(false),
    _color(1,0,0),
    _color_covered(0.8,0,0,0.5),
    _line_width(3)
  {

  }

  //----------------------------------------------------------------------------
  void NodeRenderer::setUseStencil(bool use)
  {
    _use_stencil = use;
  }

  //----------------------------------------------------------------------------
  void NodeRenderer::setColor( Color const& color,
                               Color const& color_covered )
  {
    _color = color;
    _color_covered = color_covered;
  }

  //----------------------------------------------------------------------------
  void NodeRenderer::setLineWidth(float w)
  {
    _line_width = w;
  }

  //----------------------------------------------------------------------------
  bool NodeRenderer::renderNodes( const LinkDescription::nodes_t& nodes,
                                  HyperEdgeQueue* hedges_open,
                                  HyperEdgeSet* hedges_done,
                                  bool render_all,
                                  int pass,
                                  bool do_transform )
  {
    if( _use_stencil )
    {
      // Mask every region covered be a highlight to prevent drawing links on top
      glStencilFunc(GL_ALWAYS, 1, 1);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    bool rendered_anything = false;

    if( pass > 0 && do_transform )
    {
      // First pass is used by xray previews which are given in absolute
      // coordinates.
      float2 offset = float2();
      if( !nodes.empty() && nodes.front()->getParent() )
        offset = nodes.front()->getParent()->get<float2>("screen-offset");

      glPushMatrix();
      glTranslatef(offset.x, offset.y, 0);
    }

    for( auto node = nodes.begin(); node != nodes.end(); ++node )
    {
      bool hover = (*node)->get<bool>("hover");
      float alpha = (*node)->get<float>("alpha", hover ? 1 : 0);
      if( alpha > 0.01 )
        hover = true;

      if( !hover && (*node)->get<bool>("hidden") && !render_all )
        continue;

      if( hedges_open )
      {
        for(auto child = (*node)->getChildren().begin();
                 child != (*node)->getChildren().end();
               ++child )
          hedges_open->push( child->get() );
      }

      if(    (*node)->getVertices().empty()
          || (render_all && !(*node)->get<bool>("show-in-preview", true)) )
        continue;

      if( pass == 0 )
      {
        if( !render_all && hover )
        {
          Color c = alpha * _color;
          const Rect rp = (*node)->get<Rect>("covered-preview-region");
          renderRect(rp, 2.f, 0, 0.05 * c, 2 * c);
          const Rect r = (*node)->get<Rect>("covered-region");
          renderRect(r, 3.f, 0, 0.15 * c, 2 * c);
          rendered_anything = true;
        }
        continue;
      }

      Color color_cur = !render_all && ( (   (*node)->get<bool>("covered")
                                         && !(*node)->get<bool>("hover")
                                         )
                                       || (*node)->get<bool>("outside")
                                       )
                      ? _color_covered
                      : _color;

      if( (*node)->get<bool>("outline-title") )
        color_cur *= 0.5;

      bool filled = (*node)->get<bool>("filled", false);
      if( !filled && !render_all && !(*node)->get<bool>("outline-only") )
      {
        Color light(0,0,0,0);// = 0.5 * color_cur;
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
                                               _line_width,
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

    if( pass > 0 && do_transform )
    {
      glPopMatrix();
    }

    return rendered_anything;
  }

  //----------------------------------------------------------------------------
  bool NodeRenderer::renderRect( Rect const& rect,
                                 size_t b,
                                 unsigned int tex,
                                 Color const& fill,
                                 Color const& border )
  {
    if( b )
    {
      glColor4fv(border);
      glBegin(fill.a < 0.5 ? GL_LINE_LOOP : GL_QUADS);
        glVertex2f(rect.pos.x - b,               rect.pos.y - b);
        glVertex2f(rect.pos.x + rect.size.x + b, rect.pos.y - b);
        glVertex2f(rect.pos.x + rect.size.x + b, rect.pos.y + rect.size.y + b);
        glVertex2f(rect.pos.x - b,               rect.pos.y + rect.size.y + b);
      glEnd();
    }

    if( fill.a < 0.05 )
      return b > 0;

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
  float2 NodeRenderer::glVertex2f(float x, float y)
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

      x -= _margin_left;
    }
#endif
    ::glVertex2f(x, y);
    return float2(x, y);
  }

} // namespace LinksRouting
