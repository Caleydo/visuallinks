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
      for( auto cur_reg = regions.begin();
           cur_reg != regions.end();
           ++cur_reg )
      {
        const std::string title =
          QxtWindowSystem::windowTitle(cur_reg->first).toStdString();

        auto old_reg = _regions.find(cur_reg->first);
        if( old_reg == _regions.end() )
        {
          std::cout << "New (" << title << ") "
                    << cur_reg->second
                    << std::endl;
          timeout = 2;
          continue;
        }

        if( cur_reg->second != old_reg->second )
        {
          std::cout << "Chg (" << title << ") "
                    << cur_reg->second
                    << std::endl;
          timeout = 2;
        }
      }
      if( timeout >= 0 )
      {
        if( timeout == 0 )
          emit regionsChanged();
        timeout -= 1;
      }

      _regions = regions;
      usleep(400);
    }
  }

  //----------------------------------------------------------------------------
  WindowMonitor::WindowRegions WindowMonitor::getWindows() const
  {
    WindowRegions regions;
    foreach(WId id, QxtWindowSystem::windows())
      regions[ id ] = QxtWindowSystem::windowGeometry(id);
    return regions;
  }
} // namespace LinksRouting
