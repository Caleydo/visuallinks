/*
 * window_monitor.hpp
 *
 *  Created on: 17.07.2012
 *      Author: tom
 */

#ifndef WINDOW_MONITOR_HPP_
#define WINDOW_MONITOR_HPP_

#include <QxtGui/qxtwindowsystem.h>
#include <QThread>

#include <iostream>
#include <map>

namespace LinksRouting
{
  struct WindowInfo
  {
    WId         id;
    QRect       region;
    std::string title;

    WindowInfo(WId id, const QRect& region, const std::string& title):
      id(id),
      region(region),
      title(title)
    {}

    bool operator==(const WindowInfo& rhs) const
    {
      // We don't care about title changes -> Only compare id and region
      return id == rhs.id && region == rhs.region;
    }
  };

  class WindowMonitor:
    public QThread
  {
    Q_OBJECT

    signals:
      void regionsChanged();

    protected:

      typedef std::vector<WindowInfo> WindowRegions;
      WindowRegions _regions;

      void run();

      WindowRegions getWindows() const;
  };

} // namespace LinksRouting

#endif /* WINDOW_MONITOR_HPP_ */
