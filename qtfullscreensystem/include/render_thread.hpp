/*!
 * @file render_thread.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 09.02.2012
 */

#ifndef _RENDER_THREAD_HPP_
#define _RENDER_THREAD_HPP_

#include <QThread>

namespace qtfullscreensystem
{

  class GLWidget;

  class RenderThread:
    public QThread
  {

    public:

      explicit RenderThread(GLWidget* gl_widget);

      void resize(int w, int h);
      void run();
      void stop();

    private:

      GLWidget *_gl_widget;
      int       _w, _h;

      bool      _do_render,
                _do_resize;

  };

} // namespace qtfullscreensystem

#endif /* _RENDER_THREAD_HPP_ */
