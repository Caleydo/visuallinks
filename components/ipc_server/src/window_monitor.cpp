/*
 * window_monitor.cpp
 *
 *  Created on: 17.07.2012
 *      Author: tom
 */

#include "window_monitor.hpp"
#include "log.hpp"
#include "qt_helper.hxx"
#include "JSONParser.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>
#include <QProcess>

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
  bool WindowRegions::hit( const WindowInfos::const_iterator& first_above,
                           const QPoint& point ) const
  {
    for(auto it = first_above; it != _windows.end(); ++it)
      if( it->region.contains(point) )
        return true;
    return false;
  }

  //----------------------------------------------------------------------------
  WindowMonitor::WindowMonitor( const QWidget* own_widget,
                                RegionsCallback cb_regions_changed ):
    _own_widget(own_widget),
	_timeout(-1),
	_cb_regions_changed(cb_regions_changed),
	_launcher_size(0)
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
    QRect desktop = WindowRegions(_last_regions).desktopRect();
    WId maximized_wid = 0;

    // Now get the actual list of windows
    WindowInfos regions;
    foreach(WId id, QxtWindowSystem::windows())
    {
      if( id == _own_widget->winId() )
        continue;

      QRect region = QxtWindowSystem::windowGeometry(id);

      if( QxtWindowSystem::windowTitle(id) == "unity-launcher" )
      {
        int launcher_size = region.width() - 17;
        if( _launcher_size != launcher_size )
        {
          _launcher_size = launcher_size;
          // TODO notify?
        }
        continue;
      }

      // Ignore small regions (tooltips, etc.) and not visible regions
      if(   region.width() <= 64
         || region.height() <= 64
         || region.width() * region.height() <= 8192
         || region.right() <= 0 )
        continue;

      regions.push_back(WindowInfo(
        id,
        QxtWindowSystem::isVisible(id),
        region,
        QxtWindowSystem::windowTitle(id)
      ));

      QRect visible_region = desktop.intersected(region);
      if(    visible_region.left() <= 0.08 * desktop.width()
          && visible_region.right() >= 0.94 * desktop.width()
          && visible_region.top() <= 0.06 * desktop.height()
          && visible_region.bottom() >= 0.96 * desktop.height() )
        maximized_wid = id;
    }

    if( maximized_wid )
    {
      // Mark all windows below maximized window as minimized
      for(WindowInfo& winfo: regions)
        if( maximized_wid != winfo.id )
          winfo.minimized = true;
        else
          break;
    }

#ifdef __linux__
    const int LAUNCHER_PADDING = 11;
    const int LAUNCHER_FIRST_OFFSET = 32
                                    // Dash icon
                                    + LAUNCHER_PADDING + _launcher_size;
    int pos_y = LAUNCHER_FIRST_OFFSET;

    for(QString const& launcher: _launchers)
    {
      for(WindowInfo& winfo: regions)
      {
        if( launcher.contains
            (
              winfo.title.split(' ', QString::SkipEmptyParts).last(),
              Qt::CaseInsensitive
            ) )
        {
          winfo.region_launcher =
            QRect(9, pos_y, _launcher_size, _launcher_size);
        }
      }
      pos_y += LAUNCHER_PADDING + _launcher_size;
    }
#endif

    return regions;
  }

  //----------------------------------------------------------------------------
  void WindowMonitor::check()
  {
#ifdef __linux__
    // Get list of pinned apps in the Ubuntu Unity launcher
    QString program = "/usr/bin/dconf";
    QStringList arguments;
    arguments << "read" << "/com/canonical/unity/launcher/favorites";
    QProcess dconf;
    dconf.start(program, arguments);
#endif

    WindowInfos regions = getWindowInfos();
    if( regions != _last_regions )
      _timeout = 2;
    _last_regions = regions;

    if( regions == _regions )
      _timeout = -1;

#ifdef __linux__
    dconf.waitForFinished();
    JSONParser favorites(dconf.readAllStandardOutput());
    QStringList launchers =
      favorites.getRoot().getValue<QStringList>()
                         .replaceInStrings(".desktop", "")
                         .replaceInStrings("application://", "");

    if( launchers != _launchers )
    {
      _launchers = launchers;
      _cb_regions_changed( getWindows() );
    }
#endif

    if( _timeout >= 0 )
    {
      if( _timeout == 0 )
      {
        _regions = regions;

        LOG_INFO("Trigger reroute...");

        for( auto reg = regions.begin(); reg != regions.end(); ++reg )
        {
          std::cout << "(" << reg->id << ' ' << reg->minimized << ") "
                    << reg->title
                    << ", launcher = " << reg->region_launcher
                    << ", reg = " << reg->region
                    << std::endl;
        }

        _cb_regions_changed( WindowRegions(regions) );
      }
      _timeout -= 1;
    }
  }

} // namespace LinksRouting
