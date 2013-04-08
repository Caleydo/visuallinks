/*
 * ClientInfo.cxx
 *
 *  Created on: Apr 8, 2013
 *      Author: tom
 */

#include "ClientInfo.hxx"

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  ClientInfo::ClientInfo(WId wid):
    _window_info(wid)
  {

  }

  //----------------------------------------------------------------------------
  void ClientInfo::setWindowId(WId wid)
  {
    _window_info.id = wid;
  }

  //----------------------------------------------------------------------------
  bool ClientInfo::updateWindow(const WindowRegions& windows)
  {
    WId wid = _window_info.id;
    auto window_info = std::find_if
    (
      windows.begin(),
      windows.end(),
      [wid](const WindowInfo& winfo)
      {
        return winfo.id == wid;
      }
    );

    if( window_info == windows.end() )
      return false;

    _window_info = *window_info;

    return true;
  }

  //----------------------------------------------------------------------------
  const WindowInfo& ClientInfo::getWindowInfo() const
  {
    return _window_info;
  }

  //----------------------------------------------------------------------------
  QRect ClientInfo::getViewportAbs() const
  {
    return viewport.translated( _window_info.region.topLeft() );
  }

  //----------------------------------------------------------------------------
  void ClientInfo::activateWindow()
  {
    if( _window_info.isValid() )
      QxtWindowSystem::activeWindow( _window_info.id );
  }

} // namespace LinksRouting
