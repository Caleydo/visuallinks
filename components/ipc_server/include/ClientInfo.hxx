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
#include "window_monitor.hpp"

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

    Partitions           partitions_src,
                         partitions_dest;
    HierarchicTileMapPtr tile_map,
                         tile_map_uncompressed;

    explicit ClientInfo(IPCServer* ipc_server = nullptr, WId wid = -1);

    void setWindowId(WId wid);
    const WindowInfo& getWindowInfo() const;

    /**
     * Parse and update scroll region
     */
    void parseScrollRegion( const JSONParser& json );

    void setScrollPos( const QPoint& offset );

    /**
     * Parse regions from JSON and replace current regions
     */
    LinkDescription::NodePtr parseRegions(const JSONParser& json);

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

      uint32_t                      _dirty;
      IPCServer                    *_ipc_server;
      WindowInfo                    _window_info;
      LinkDescription::nodes_t      _nodes;
      LinkDescription::NodePtr      _minimized_icon;
      float                         _avg_region_height;

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
       * Update tile map (and recalculate paritions)
       */
      void updateTileMap();

      WindowList::const_iterator findWindow( const WindowRegions& windows,
                                             WId wid ) const;
  };

} // namespace LinksRouting

#endif /* CLIENTINFO_HXX_ */
