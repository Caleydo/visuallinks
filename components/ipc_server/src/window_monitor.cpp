/*
 * window_monitor.cpp
 *
 *  Created on: 17.07.2012
 *      Author: tom
 */

#include "window_monitor.hpp"

namespace LinksRouting
{
  //----------------------------------------------------------------------------
  std::ostream& operator<<(std::ostream& strm, const QRect& r)
  {
    return strm << "(" << r.x() << "|" << r.y() << ") "
                << r.width() << "x" << r.height();
  }

  //----------------------------------------------------------------------------
  void WindowMonitor::run()
  {
    int timeout = -1;
    for(;;)
    {
      WindowRegions regions = getWindows();
      if( regions != _regions )
      {
        _regions = regions;
        timeout = 2;
      }

//      for( auto cur_reg = regions.begin();
//           cur_reg != regions.end();
//           ++cur_reg )
//      {
//        auto old_reg = _regions.find(cur_reg->first);
//        if( old_reg == _regions.end() )
//        {
//          std::cout << "New (" << title << ") "
//                    << cur_reg->second
//                    << std::endl;
//          timeout = 2;
//          continue;
//        }
//
//        if( cur_reg->second != old_reg->second )
//        {
//          std::cout << "Chg (" << title << ") "
//                    << cur_reg->second
//                    << std::endl;
//          timeout = 2;
//        }
//      }
      if( timeout >= 0 )
      {
        if( timeout == 0 )
        {
          std::cout << "Reroute..." << std::endl;
          for( auto reg = regions.begin(); reg != regions.end(); ++reg )
            std::cout << reg->title << " -> " << reg->region << std::endl;
          emit regionsChanged();
        }
        timeout -= 1;
      }

      usleep(300);
    }
  }

  //----------------------------------------------------------------------------
  WindowMonitor::WindowRegions WindowMonitor::getWindows() const
  {
    WindowRegions regions;
    foreach(WId id, QxtWindowSystem::windows())
    {
      QRect region = QxtWindowSystem::windowGeometry(id);

      // Ignore small regions (tooltips, etc.) and not visible regions
      if(   region.width() <= 64
         || region.height() <= 64
         || region.width() * region.height() <= 8192
         || region.right() <= 0 )
        continue;

      regions.push_back(WindowInfo(
        id,
        region,
        QxtWindowSystem::windowTitle(id).toStdString()
      ));
    }
    return regions;
  }
} // namespace LinksRouting
