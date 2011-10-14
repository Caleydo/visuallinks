#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QGLWidget>

class GLWidget: public QGLWidget
{
  public:

    GLWidget(QWidget *parent = 0);
    virtual ~GLWidget();

    /**
     * Set a flag to show that the following renderings will be used for
     * creating a mask for the window.
     *
     * When set the rendering doesn't need to use any shaders/textures/etc. but
     * only render the shapes of all rendered objects.
     *
     */
    void setRenderMask();

    /**
     * Clear the flag indicating rendering for a mask. See #setRenderMask
     */
    void clearRenderMask();


  protected:

    // QT OpenGL callbacks
    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);

  private:

    bool    _render_mask;

};

#endif
