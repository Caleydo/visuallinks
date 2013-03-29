/*
 * text_popup.hpp
 *
 *  Created on: 20.09.2012
 *      Author: tom
 */

#ifndef TEXT_POPUP_HPP_
#define TEXT_POPUP_HPP_

#include "../float2.hpp"
#include "../linkdescription.h"
#include "../HierarchicTileMap.hpp"

#include <string>
#include <vector>

namespace LinksRouting
{
namespace SlotType
{

  struct TextPopup
  {
    struct HoverRect
    {
      Rect region, src_region, scroll_region;
      float border;
      bool visible;
      float2 offset, dim;
      int zoom;
      HierarchicTileMapWeakPtr tile_map;

      HoverRect( const float2& pos,
                 const float2& size,
                 float border,
                 bool visible,
                 int zoom = 0):
        region(pos, size),
        border(border),
        visible(visible),
        zoom(zoom)
      {}

      bool contains(float x, float y) const
      {
        return region.contains(x,y, border);
      }
    };
    struct Popup
    {
      std::string text;
      LinkDescription::nodes_t nodes;
      HoverRect region;
      HoverRect hover_region;
      bool auto_resize;
    };

    typedef std::vector<Popup> Popups;
    Popups popups;
  };

  struct XRayPopup
  {
    unsigned int tex_id;
    Rect region;
  };

} // namespace SlotType
} // namespace LinksRouting


#endif /* TEXT_POPUP_HPP_ */
