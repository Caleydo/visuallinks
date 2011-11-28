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

    protected:

      // QT OpenGL callbacks
      virtual void initializeGL();
      virtual void paintGL();
      virtual void resizeGL(int width, int height);

    private:

      std::shared_ptr<QGLFramebufferObject> _fbo_links;
      std::shared_ptr<QGLFramebufferObject> _fbo_desktop;

  };
} // namespace qtfullscreensystem

#endif /* _GLWIDGET_HPP_ */
