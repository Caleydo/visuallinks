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
#include <QTimer>

#include <functional>
#include <iostream>
#include <map>

namespace LinksRouting
{
  struct WindowInfo
  {
    WId         id;
    QRect       region;
    std::string title;

    explicit WindowInfo( WId id,
                         const QRect& region = QRect(),
                         const std::string& title = "" ):
      id(id),
      region(region),
      title(title)
    {}

    bool operator==(const WindowInfo& rhs) const
    {
      // We don't care about title changes -> Only compare id and region
      return id == rhs.id && region == rhs.region;
    }

    bool operator!=(const WindowInfo& rhs) const
    {
      return !(*this == rhs);
    }

    bool isValid() const
    {
      return id && id != static_cast<WId>(-1);
    }
  };
  typedef std::vector<WindowInfo> WindowInfos;

  class WindowRegions
  {
    public:
      WindowRegions(const WindowInfos& windows);

      WindowInfos::const_iterator find(WId wid) const;
      WindowInfos::const_iterator begin() const;
      WindowInfos::const_iterator end() const;

      QRect desktopRect() const;
      WindowInfos::const_reverse_iterator windowAt(const QPoint& point) const;
      WId windowIdAt(const QPoint& point) const;

    private:
      const WindowInfos _windows;
  };

  class WindowMonitor:
    public QThread
  {
    Q_OBJECT

    public:

      typedef std::function<void(const WindowRegions&)> RegionsCallback;

      /**
       * @param own_id  id of own window where links are renderd (will be
       *                ignored)
       */
      WindowMonitor( const QWidget* own_widget,
                     RegionsCallback cb_regions_changed );


      /**
       * Get all visible windows in stacking order and filtered by minimum size
       */
      WindowRegions getWindows() const;

    protected:

      const QWidget *_own_widget;
      WindowInfos    _regions,
		             _last_regions;
	  int            _timeout;
      QTimer         _timer;
      RegionsCallback _cb_regions_changed;

      /**
       * Get all visible windows in stacking order and filtered by minimum size
       */
      WindowInfos getWindowInfos() const;

	protected slots:
      void check();

  };

  std::ostream& operator<<(std::ostream& strm, const QRect& r);

} // namespace LinksRouting

#endif /* WINDOW_MONITOR_HPP_ */
