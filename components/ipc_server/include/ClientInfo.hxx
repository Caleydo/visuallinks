/*
 * ClientInfo.hxx
 *
 *  Created on: Apr 8, 2013
 *      Author: tom
 */

#ifndef CLIENTINFO_HXX_
#define CLIENTINFO_HXX_

#include "HierarchicTileMap.hpp"
#include "PartitionHelper.hxx"
#include "window_monitor.hpp"
#include <QRect>

namespace LinksRouting
{

  struct ClientInfo
  {
    /** Viewport (relative to application window) */
    QRect   viewport,

    /** Scroll region (relative to application viewport) */
            scroll_region,

            scroll_region_uncompressed;
    Partitions           partitions_src,
                         partitions_dest;
    HierarchicTileMapPtr tile_map,
                         tile_map_uncompressed;

    ClientInfo(WId wid = -1);
    void setWindowId(WId wid);

    bool updateWindow(const WindowRegions& windows);
    const WindowInfo& getWindowInfo() const;

    /**
     * Get viewport in absolute (screen) coordinates
     */
    QRect getViewportAbs() const;

    /**
     * Activate window (and raise to top)
     */
    void activateWindow();

    private:
      WindowInfo _window_info;
  };

} // namespace LinksRouting

#endif /* CLIENTINFO_HXX_ */
