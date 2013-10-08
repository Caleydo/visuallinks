/*!
 * @file application.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 14.10.2011
 */

#include "application.hpp"

#include <QDesktopWidget>
#include <QElapsedTimer>
#include <iostream>

namespace qtfullscreensystem
{

  //----------------------------------------------------------------------------
  Application::Application(int& argc, char *argv[]):
    QApplication(argc, argv),
    _gl_widget(argc, argv)
  {
	_gl_widget.show();
	_gl_widget.startRender();
  }

  //----------------------------------------------------------------------------
  Application::~Application()
  {

  }

  //----------------------------------------------------------------------------
  bool Application::notify(QObject* receiver, QEvent* e)
  {
    switch( e->type() )
    {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonRelease:
      case QEvent::MouseButtonDblClick:
      case QEvent::MouseMove:
      case QEvent::Wheel:
        return true;
      default:
        return QApplication::notify(receiver, e);
    }
  }

} // namespace qtfullscreensystem
