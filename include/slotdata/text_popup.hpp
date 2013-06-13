/*
 * text_popup.hpp
 *
 *  Created on: 20.09.2012
 *      Author: tom
 */

#ifndef TEXT_POPUP_HPP_
#define TEXT_POPUP_HPP_

#include "../clock.hxx"
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
      clock::time_point hide_time;
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
      std::string link_id;

      /** Region to check for mouse over */
      Rect region,

      /** Region to show preview within */
           preview_region,

      /** Source region within document/scrollable region */
           source_region;

      LinkDescription::NodePtr node;
      HierarchicTileMapWeakPtr tile_map;
      void* client_socket;
    };

    typedef std::list<HoverRect> Popups;
    Popups popups;
  };

  struct CoveredOutline
  {
    struct Outline
    {
      std::string   title;
      Rect          region_title,
                    region_outline;
      XRayPopup::Popups::iterator preview;
      bool preview_valid;
    };

    typedef std::list<Outline> List;
    List popups;
  };

} // namespace SlotType
} // namespace LinksRouting


#endif /* TEXT_POPUP_HPP_ */
