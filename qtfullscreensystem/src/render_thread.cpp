/*!
 * @file render_thread.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 09.02.2012
 */

#include "render_thread.hpp"
#include "clock.hxx"
#include "qglwidget.hpp"

#include <QGLPixelBuffer>

namespace qtfullscreensystem
{

  //----------------------------------------------------------------------------
  RenderThread::RenderThread( GLWidget* gl_widget,
                              int w,
                              int h ):
    _gl_widget( gl_widget ),
    _w(0),
    _h(0),
    _do_render( false ),
    _do_resize( false )
  {
    if( !QGLFormat::hasOpenGL() )
      qFatal("OpenGL not supported!");

    if( !QGLFramebufferObject::hasOpenGLFramebufferObjects() )
      qFatal("OpenGL framebufferobjects not supported!");

    if( !QGLPixelBuffer::hasOpenGLPbuffers() )
      qFatal("OpenGL pbuffer not supported!");

    QGLFormat format;
    format.setAlpha(true);
    _pbuffer =
      QSharedPointer<QGLPixelBuffer>(new QGLPixelBuffer(QSize(w, h), format));

    if( !_pbuffer->isValid() )
      qFatal("Unable to create OpenGL context (not valid)");

    qDebug
    (
      "Created pbuffer with OpenGL %d.%d",
      _pbuffer->format().majorVersion(),
      _pbuffer->format().minorVersion()
    );
  }

  //----------------------------------------------------------------------------
  void RenderThread::resize(int w, int h)
  {
    _w = w;
    _h = h;
    _do_resize = true;
  }

  //----------------------------------------------------------------------------
  void RenderThread::run()
  {
    assert(_gl_widget);

    _do_render = true;

    //_gl_widget->makeCurrent();
    _pbuffer->makeCurrent();
    _gl_widget->setupGL();

//    int screenshot_counter = 0;
    do
    {
      //std::cout << "--- FRAME --- ----------------------------------" << std::endl;
      PROFILE_START()

      if( _do_resize )
      {
        glViewport(0,0,_w,_h);
        //glOrtho(0, static_cast<float>(w)/h, 0, 1, -1.0, 1.0);
        _do_resize = false;
      }

#ifdef USE_GPU_ROUTING
      _gl_widget->render(0, _pbuffer.data());
      //std::cout << "done" << _pbuffer->doneCurrent() << std::endl;
      //_gl_widget->swapBuffers();

      PROFILE_LAP("* pass #0")
#endif

      _gl_widget->render(1, _pbuffer.data());

      PROFILE_LAP("* pass #1")

#ifdef USE_GPU_ROUTING
      //_gl_widget->swapBuffers();
      //_gl_widget->doneCurrent();
      msleep(10);
      /*if( --screenshot_counter < 0 )
      {*/
        _gl_widget->captureScreen();
        /*screenshot_counter = 5;
      }*/
#endif

      PROFILE_LAP("* total")

      _gl_widget->waitForData();
      //_gl_widget->makeCurrent();

    } while( _do_render );
  }

  //----------------------------------------------------------------------------
  void RenderThread::stop()
  {
    _do_render = false;
  }

} // namespace qtfullscreensystem
