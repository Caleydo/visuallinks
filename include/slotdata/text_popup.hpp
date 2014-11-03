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
  class PreviewWindow;

namespace SlotType
{
  class AnimatedPopup
  {
    public:
      static const double SHOW_DELAY;
      static const double HIDE_DELAY;
      static const double TIMEOUT;

      static bool debug;

      AnimatedPopup(bool visible = true);

      /**
       * @param dt  Time delta
       * @return Whether further updates are needed
       */
      bool update(double dt);

      void show();
      void hide();
      void fadeIn();
      void fadeOut();
      void delayedFadeIn();
      void delayedFadeOut();

      double getAlpha() const;
      bool isVisible() const;
      bool isTransient() const;
      bool isFadeIn() const;
      bool isFadeOut() const;

    protected:
      enum Flags
      {
        HIDDEN = 0,
        VISIBLE = 1,
        TRANSIENT = VISIBLE << 1,
        DELAYED = TRANSIENT << 1
      };

      uint16_t  _state;
      double    _time;

      void setFlags(uint16_t flags);
  };

  struct TextPopup
  {
    struct HoverRect:
      public AnimatedPopup
    {
      Rect region, src_region, scroll_region;
      float border;
      float2 offset, dim;
      int zoom;
      HierarchicTileMapWeakPtr tile_map;

      HoverRect( const float2& pos,
                 const float2& size,
                 float border,
                 bool visible,
                 int zoom = 0):
        AnimatedPopup(visible),
        region(pos, size),
        border(border),
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
      LinkDescription::NodePtr node;
      PreviewWindow *preview;

      void* client_socket;
      HoverRect region;
      HoverRect hover_region;
      bool auto_resize;

      ~Popup();
    };

    typedef std::list<Popup> Popups;
    Popups popups;
  };

  struct XRayPopup
  {
    struct HoverRect:
      public AnimatedPopup
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

      HoverRect():
        AnimatedPopup(false),
        client_socket(0)
      {}
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
