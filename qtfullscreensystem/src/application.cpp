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

    //connect(&_timer, SIGNAL(timeout()), this, SLOT(timeOut()));
    //_timer.start(100);
  }

  //----------------------------------------------------------------------------
  Application::~Application()
  {

  }

  //----------------------------------------------------------------------------
  void Application::timeOut()
  {
    static int capture = 2;

    switch( capture )
    {
      case 0:
        _gl_widget.captureScreen();
        capture = 3;
        break;
      case 2:
        _gl_widget.updateGL();
        break;
    }

    --capture;
  }

} // namespace qtfullscreensystem
