/*
 * window_monitor.cpp
 *
 *  Created on: 17.07.2012
 *      Author: tom
 */

#include "window_monitor.hpp"
#include "log.hpp"
#include "qt_helper.hxx"

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
  WindowRegions::WindowRegions(const WindowInfos& windows):
    _windows(windows)
  {

  }

  //----------------------------------------------------------------------------
  WindowInfos::const_iterator WindowRegions::find(WId wid) const
  {
    return std::find_if
    (
      _windows.begin(),
      _windows.end(),
      [wid](const WindowInfo& winfo)
      {
        return winfo.id == wid;
      }
    );
  }

  //----------------------------------------------------------------------------
  WindowInfos::const_iterator WindowRegions::begin() const
  {
    return _windows.begin();
  }

  //----------------------------------------------------------------------------
  WindowInfos::const_iterator WindowRegions::end() const
  {
    return _windows.end();
  }

  //----------------------------------------------------------------------------
  QRect WindowRegions::desktopRect() const
  {
    return _windows.empty() ? QRect() : _windows.front().region;
  }

  //----------------------------------------------------------------------------
  WindowInfos::const_reverse_iterator
  WindowRegions::windowAt(const QPoint& pos) const
  {
    for( auto reg = _windows.rbegin(); reg != _windows.rend(); ++reg )
    {
      if( reg->region.contains(pos) )
        return reg;
    }
    return _windows.rend();
  }

  //----------------------------------------------------------------------------
  WId WindowRegions::windowIdAt(const QPoint& point) const
  {
    auto window = windowAt(point);
    if( window != _windows.rend() )
      return window->id;

    return 0;
  }

  //----------------------------------------------------------------------------
  WindowMonitor::WindowMonitor( const QWidget* own_widget,
                                RegionsCallback cb_regions_changed ):
    _own_widget(own_widget),
	_timeout(-1),
	_cb_regions_changed(cb_regions_changed)
  {
	connect(&_timer, SIGNAL(timeout()), this, SLOT(check()));
	_timer.start(150);
  }

  //----------------------------------------------------------------------------
  WindowRegions WindowMonitor::getWindows() const
  {
    return WindowRegions( getWindowInfos() );
  }

  //----------------------------------------------------------------------------
  WindowInfos WindowMonitor::getWindowInfos() const
  {
    WindowInfos regions;
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
        to_string(QxtWindowSystem::windowTitle(id))
      ));
    }
    return regions;
  }

  //----------------------------------------------------------------------------
  void WindowMonitor::check()
  {
    WindowInfos regions = getWindowInfos();
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

        _cb_regions_changed( WindowRegions(regions) );
      }
      _timeout -= 1;
    }
  }

} // namespace LinksRouting
