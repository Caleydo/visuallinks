/*!
 * @file render_thread.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 09.02.2012
 */

#include "render_thread.hpp"
#include "qglwidget.hpp"

namespace qtfullscreensystem
{

  //----------------------------------------------------------------------------
  RenderThread::RenderThread(GLWidget* gl_widget):
    _gl_widget( gl_widget ),
    _do_render( false ),
    _do_resize( false )
  {

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
    _gl_widget->makeCurrent();
    _gl_widget->initGL();

    int screenshot_counter = 0;
    do
    {
      if( _do_resize )
      {
        glViewport(0,0,_w,_h);
        //glOrtho(0, static_cast<float>(w)/h, 0, 1, -1.0, 1.0);
        _do_resize = false;
      }

      _gl_widget->render();
      _gl_widget->swapBuffers();
      msleep(100);
      if( --screenshot_counter < 0 )
      {
        _gl_widget->captureScreen();
        screenshot_counter = 5;
      }
      msleep(100);
    } while( _do_render );
  }

  //----------------------------------------------------------------------------
  void RenderThread::stop()
  {
    _do_render = false;
  }

} // namespace qtfullscreensystem
