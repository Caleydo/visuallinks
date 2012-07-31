/*
 * window_monitor.cpp
 *
 *  Created on: 17.07.2012
 *      Author: tom
 */

#include "window_monitor.hpp"
#include "log.hpp"

#include <QApplication>
#include <QDesktopWidget>

namespace LinksRouting
{
  //----------------------------------------------------------------------------
  std::ostream& operator<<(std::ostream& strm, const QRect& r)
  {
    return strm << "(" << r.x() << "|" << r.y() << ") "
                << r.width() << "x" << r.height();
  }

  //----------------------------------------------------------------------------
  WindowMonitor::WindowMonitor(const QWidget* own_widget):
    _own_widget(own_widget),
	_timeout(-1)
  {
	connect(&_timer, SIGNAL(timeout()), this, SLOT(check()));
	_timer.start(300);
  }

  //----------------------------------------------------------------------------
  WindowRegions WindowMonitor::getWindows() const
  {
    WindowRegions regions;
    foreach(WId id, QxtWindowSystem::windows())
    {
      if( id == _own_widget->winId() )
        continue;

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

  //----------------------------------------------------------------------------
  void WindowMonitor::check()
  {
    WindowRegions regions = getWindows();
    if( regions != _last_regions )
    _timeout = 2;
    _last_regions = regions;

    if( regions == _regions )
    _timeout = -1;

    if( _timeout >= 0 )
    {
    if( _timeout == 0 )
    {
        _regions = regions;

        LOG_INFO("Trigger reroute...");

        for( auto reg = regions.begin(); reg != regions.end(); ++reg )
        std::cout << "(" << reg->id << ") "
                    << reg->title << " -> " << reg->region << std::endl;

        emit regionsChanged(regions);
    }
    _timeout -= 1;
    }
  }

} // namespace LinksRouting
