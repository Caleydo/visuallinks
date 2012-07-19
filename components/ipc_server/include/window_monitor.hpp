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
  class WindowMonitor:
    public QThread
  {
    Q_OBJECT

    signals:
      void regionsChanged();

    protected:

      typedef std::map<WId, QRect> WindowRegions;
      WindowRegions _regions;

      void run();

      WindowRegions getWindows() const;
  };

} // namespace LinksRouting

#endif /* WINDOW_MONITOR_HPP_ */
