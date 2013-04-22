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

class QImage;

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
      std::string text,
                  link_id;
      LinkDescription::nodes_t nodes;
      void* client_socket;
      HoverRect region;
      HoverRect hover_region;
      bool auto_resize;
    };

    typedef std::list<Popup> Popups;
    Popups popups;
  };

  struct XRayPopup
  {
    struct HoverRect
    {
      Rect region, preview_region;
      LinkDescription::Node* node;
      QImage* img;
      float2 pos;
    };

    typedef std::list<HoverRect> Popups;
    Popups popups;
  };

} // namespace SlotType
} // namespace LinksRouting


#endif /* TEXT_POPUP_HPP_ */
