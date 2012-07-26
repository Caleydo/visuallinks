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
  typedef std::vector<WindowInfo> WindowRegions;

  class WindowMonitor:
    public QThread
  {
    Q_OBJECT

    public:

      /**
       * @param own_id  id of own window where links are renderd (will be
       *                ignored)
       */
      WindowMonitor(const QWidget* own_widget);


      /**
       * Get all visible windows in stacking order and filtered by minimum size
       */
      WindowRegions getWindows() const;

    signals:
      void regionsChanged(WindowRegions regions);

    protected:

      const QWidget *_own_widget;
      WindowRegions  _regions;
      void run();

  };

} // namespace LinksRouting

#endif /* WINDOW_MONITOR_HPP_ */
