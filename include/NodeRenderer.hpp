/*
 * NodeRenderer.hpp
 *
 *  Created on: Nov 3, 2014
 *      Author: tom
 */

#ifndef LR_NODERENDERER_HPP_
#define LR_NODERENDERER_HPP_

#include "PartitionHelper.hxx"
#include "linkdescription.h"

#include <queue>
#include <set>

namespace LinksRouting
{

  typedef std::queue<const LinkDescription::HyperEdge*> HyperEdgeQueue;
  typedef std::set<const LinkDescription::HyperEdge*> HyperEdgeSet;

  class NodeRenderer
  {
    public:
      explicit NodeRenderer( Partitions *partitions_src = nullptr,
                             Partitions *partitions_dest = nullptr,
                             unsigned int margin_left = 0 );

      void setUseStencil(bool use);
      void setColor( Color const& color,
                     Color const& color_covered );
      void setLineWidth(float w);

      bool renderNodes( const LinkDescription::nodes_t& nodes,
                        HyperEdgeQueue* hedges_open = nullptr,
                        HyperEdgeSet* hedges_done = nullptr,
                        bool render_all = false,
                        int pass = 0,
                        bool do_transform = true );

      bool renderRect( const Rect& rect,
                       size_t margin = 2,
                       unsigned int tex = 0,
                       const Color& fill = Color(1, 1, 1, 1),
                       const Color& border = Color(0.3, 0.3, 0.3, 0.8) );

      float2 glVertex2f(float x, float y);

    protected:
      Partitions  *_partitions_src,
                  *_partitions_dest;
      unsigned int _margin_left;
      bool         _use_stencil;
      Color        _color,
                   _color_covered;
      float        _line_width;
  };

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

      if( offset_out.length() > 2 * w_out )
        offset_out *= 2 * w_out / offset_out.length();

      if( offset_in.length() > 2 * w_in )
        offset_in *= 2 * w_in / offset_in.length();

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

} // namespace LinksRouting

#endif /* LR_NODERENDERER_HPP_ */
