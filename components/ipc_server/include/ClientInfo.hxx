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

    ClientInfo(WId wid = -1);
    void setWindowId(WId wid);

    bool updateWindow(const WindowRegions& windows);
    const WindowInfo& getWindowInfo() const;

    /**
     * Parse and update scroll region
     */
    void parseScrollRegion( const JSONParser& json );

    /**
     * Parse regions from JSON and replace current regions
     */
    LinkDescription::NodePtr parseRegions( const JSONParser& json );

    /**
     * Update tile map (and recalculate paritions)
     */
    void updateTileMap();

    /**
     * Get viewport in absolute (screen) coordinates
     */
    QRect getViewportAbs() const;

    /**
     * Activate window (and raise to top)
     */
    void activateWindow();

    /**
     * Get node (with hedge and regions)
     */
    LinkDescription::NodePtr getNode();

    private:
      WindowInfo                _window_info;
      LinkDescription::NodePtr  _node;
      float                     _avg_region_height;
  };

} // namespace LinksRouting

#endif /* CLIENTINFO_HXX_ */
