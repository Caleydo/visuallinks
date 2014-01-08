/*
 * Window for displaying (parts) of the links visualizations
 *
 *  Created on: Oct 29, 2013
 *      Author: tom
 */

#ifndef QTF_WINDOW_HPP_
#define QTF_WINDOW_HPP_

#include <QWidget>
#include <memory>

namespace qtfullscreensystem
{

  class Window:
    public QWidget
  {
    public:
      Window(const QRect& geometry);
      void setImage(QImage const* img);

    protected:
      QRect     _geometry;
      QImage const *_img;

      virtual void moveEvent(QMoveEvent *event);
      virtual void paintEvent(QPaintEvent* e);
      virtual void mouseMoveEvent(QMouseEvent *e);
  };
  typedef std::shared_ptr<Window> WindowRef;

} // namespace qtfullscreensystem


#endif /* QTF_WINDOW_HPP_ */
