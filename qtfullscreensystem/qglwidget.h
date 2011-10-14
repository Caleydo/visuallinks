#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QGLWidget>

class GLWidget: public QGLWidget
{
  public:

    GLWidget(QWidget *parent = 0);

    virtual ~GLWidget();
    void initializeGL();
    void paintGL();
    void resizeGL();

  protected:

    //virtual void moveEvent(QMoveEvent *event);
};

#endif
