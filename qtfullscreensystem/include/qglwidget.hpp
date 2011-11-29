#ifndef _GLWIDGET_HPP_
#define _GLWIDGET_HPP_

#include <QGLWidget>
#include <QGLFramebufferObject>
#include <memory>

namespace qtfullscreensystem
{
  class GLWidget: public QGLWidget
  {
    public:

      GLWidget(QWidget *parent = 0);
      virtual ~GLWidget();

      /**
       * Signal that the screen is ready for being captured
       */
      void captureScreen();

    protected:

      // QT OpenGL callbacks
      virtual void initializeGL();
      virtual void paintGL();
      virtual void resizeGL(int width, int height);

      virtual void moveEvent(QMoveEvent *event);

    private:

      std::unique_ptr<QGLFramebufferObject> _fbo_links;
      std::unique_ptr<QGLFramebufferObject> _fbo_desktop;

      QPixmap _screenshot;

  };
} // namespace qtfullscreensystem

#endif /* _GLWIDGET_HPP_ */
