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

    // transparent, always on top window without any decoration
    // TODO check windows
    _gl_widget.setWindowFlags( Qt::WindowStaysOnTopHint
                             | Qt::FramelessWindowHint
                             | Qt::MSWindowsOwnDC
                             | Qt::X11BypassWindowManagerHint
                             );
    //_gl_widget.setAttribute( Qt::WA_TranslucentBackground );
    _gl_widget.setWindowOpacity(0.7);

    // update render mask
//    _gl_widget.setRenderMask();
//    _mask = _gl_widget.renderPixmap().createMaskFromColor( QColor(0,0,0) );
//    _mask.save( QString("mask.png") );
//    _gl_widget.setMask(_mask);
//    _gl_widget.clearRenderMask();

    _gl_widget.show();

    connect(&_timer, SIGNAL(timeout()), this, SLOT(timeOut()));
    _timer.start(5000);
  }

  //----------------------------------------------------------------------------
  Application::~Application()
  {

  }

  //----------------------------------------------------------------------------
  void Application::timeOut()
  {
//    QPixmap screen = QPixmap::grabWindow(QApplication::desktop()->winId());
//    _gl_widget.update();
//    QImage screenshot = screen.toImage();
//
//    static int count = 0;
//    screen.save(QString("desktop%1.png").arg(count));
//    std::cout << count++ << std::endl;

//
//    if( screenshot != _last_screenshot )
//    {
//      std::cout << "changed" << std::endl;
//
//      _last_screenshot = screenshot;

      //_gl_widget.setMask( QApplication::desktop()->screenGeometry(&_gl_widget) );



      // update render mask
//      _gl_widget.setRenderMask();
//      _mask = _gl_widget.renderPixmap().createMaskFromColor( QColor(0,0,0) );
//
//      _mask.save( QString("mask%1.png").arg(count++) );
//      _gl_widget.clearRenderMask();
//    }

    // and show widget again...
    //_gl_widget.setMask(_mask);
    //_gl_widget.setMask( QApplication::desktop()->screenGeometry(&_gl_widget) );
    //_gl_widget.setWindowOpacity(1);
    //_gl_widget.show();
    _gl_widget.update();
  }

} // namespace qtfullscreensystem
