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
    QApplication(argc, argv)
  {
    if( !QGLFormat::hasOpenGL() )
      qFatal("OpenGL not supported!");

    if( !QGLFramebufferObject::hasOpenGLFramebufferObjects() )
      qFatal("OpenGL framebufferobjects not supported!");

    // fullscreen
    _gl_widget.setGeometry
    (
      QApplication::desktop()->screenGeometry(&_gl_widget)
    );

    // don't allow user to change the window size
    _gl_widget.setFixedSize( _gl_widget.size() );

    // transparent, always on top window without any decoration
    // TODO check windows
    _gl_widget.setWindowFlags( Qt::WindowStaysOnTopHint
                             | Qt::FramelessWindowHint
                             | Qt::MSWindowsOwnDC
                             //| Qt::X11BypassWindowManagerHint
                             );
    _gl_widget.setWindowOpacity(0.6);
	_gl_widget.show();

    connect(&_timer, SIGNAL(timeout()), this, SLOT(timeOut()));
    _timer.start(100);
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
