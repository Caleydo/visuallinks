/*
 * ClientInfo.hxx
 *
 *  Created on: Apr 8, 2013
 *      Author: tom
 */

#ifndef CLIENTINFO_HXX_
#define CLIENTINFO_HXX_

#include "HierarchicTileMap.hpp"
#include "JSONParser.h"
#include "linkdescription.h"
#include "PartitionHelper.hxx"
#include "slotdata/text_popup.hpp"
#include "window_monitor.hpp"

template<typename T>
void clamp(T& val, T min, T max)
{
  val = std::max(min, std::min(val, max));
}

namespace LinksRouting
{

  class IPCServer;
  struct ClientInfo
  {
    /** Viewport (relative to application window) */
    QRect   viewport,

    /** Scroll region (relative to application viewport) */
            scroll_region;

    /** Size of whole preview region */
    QSize   preview_size;

    HierarchicTileMapPtr tile_map,
                         tile_map_uncompressed;

    explicit ClientInfo(IPCServer* ipc_server = nullptr, WId wid = -1);
    ClientInfo(const ClientInfo&) = delete;

    void setWindowId(WId wid);
    const WindowInfo& getWindowInfo() const;

    /**
     * Parse and update scroll region
     */
    void parseScrollRegion( const JSONParser& json );

    void setScrollPos( const QPoint& offset );

    /**
     * Parse regions from JSON and replace current regions in node if given
     */
    LinkDescription::NodePtr parseRegions
    (
      const JSONParser& json,
      LinkDescription::NodePtr node = nullptr
    );

    /**
     *
     */
    bool update(const WindowRegions& windows);

    /**
     * Remove all information about given link
     */
    void removeLink(LinkDescription::HyperEdge* hedge);

    /**
     * Get viewport in absolute (screen) coordinates
     */
    QRect getViewportAbs() const;

    /**
     * Get scroll region in absolute (screen) coordinates
     */
    QRect getScrollRegionAbs() const;

    /**
     * Activate window (and raise to top)
     */
    void activateWindow();

    private:

      enum DirtyFlags
      {
        WINDOW          = 1,
        REGIONS         = WINDOW << 1,
        VISIBLITY       = REGIONS << 1,
        SCROLL_POS      = VISIBLITY << 1,
        SCROLL_SIZE     = SCROLL_POS << 1
      };

      /**
       * Summary about regions scrolled outside in a common direction
       */
      struct OutsideScroll
      {
        float2 pos;
        float2 normal;
        size_t num_outside;

        bool operator==(const OutsideScroll& rhs) const
        {
          return    num_outside == rhs.num_outside
                 && pos == rhs.pos
                 && normal == rhs.normal;
        }

        bool operator!=(const OutsideScroll& rhs) const
        {
          return !(*this == rhs);
        }
      };

      typedef SlotType::TextPopup::Popups::iterator PopupIterator;
      typedef std::list<PopupIterator> Popups;

      typedef SlotType::XRayPopup::Popups::iterator XRayIterator;
      typedef std::list<XRayIterator> XRayPreviews;

      typedef SlotType::CoveredOutline::List::iterator OutlineIterator;
      typedef std::list<OutlineIterator> Outlines;

      uint32_t                      _dirty;
      IPCServer                    *_ipc_server;
      WindowInfo                    _window_info;
      LinkDescription::nodes_t      _nodes;
      LinkDescription::NodePtr      _minimized_icon,
                                    _covered_outline;
      Popups                        _popups;
      XRayPreviews                  _xray_previews;
      Outlines                      _outlines;
      float                         _avg_region_height;

      float2 getPreviewSize() const;
      void createPopup( const float2& pos,
                        const float2& normal,
                        const std::string& text,
                        const LinkDescription::nodes_t& nodes,
                        const std::string& link_id = "",
                        bool auto_resize = true );

      void updateRegions(const WindowRegions& windows);
      bool updateNode( LinkDescription::Node& hedge,
                       const QRect& desktop, ///<! desktop in local coords
                       const QRect& view,    ///<! viewport in local coords
                       const WindowRegions& windows,
                       const WindowInfos::const_iterator& first_above );
      bool updateChildren( LinkDescription::HyperEdge& hedge,
                           const QRect& desktop, ///<! desktop in local coords
                           const QRect& view,    ///<! viewport in local coords
                           const WindowRegions& windows,
                           const WindowInfos::const_iterator& first_above );
      void updateHedges( LinkDescription::hedges_t& hedges,
                         bool first = true );

      /**
       * Update tile map (and recalculate partitions)
       */
      void updateTileMap();

      WindowList::const_iterator findWindow( const WindowRegions& windows,
                                             WId wid ) const;
  };

} // namespace LinksRouting

#endif /* CLIENTINFO_HXX_ */
