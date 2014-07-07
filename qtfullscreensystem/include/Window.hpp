/*
 * Window for displaying (parts) of the links visualizations
 *
 *  Created on: Oct 29, 2013
 *      Author: tom
 */

#ifndef QTF_WINDOW_HPP_
#define QTF_WINDOW_HPP_

#include <slots.hpp>
#include <slotdata/mouse_event.hpp>
#include <slotdata/text_popup.hpp>

#include <QWidget>
#include <memory>

namespace qtfullscreensystem
{
  namespace LR = LinksRouting;

  class RenderWindow:
    public QWidget
  {
    public:
      RenderWindow(const QRect& geometry);

      void subscribeSlots(LR::SlotSubscriber& slot_subscriber);

      void setImage(QImage const* img);

    protected:
      QRect     _geometry;
      QImage const *_img;

      bool    _do_drag;
      float2  _last_mouse_pos;

      LR::slot_t<LR::SlotType::MouseEvent>::type _subscribe_mouse;
      LR::slot_t<LR::SlotType::TextPopup>::type _subscribe_popups;
      LR::slot_t<LR::SlotType::CoveredOutline>::type _subscribe_outlines;
      LR::slot_t<LR::SlotType::XRayPopup>::type _subscribe_xray;

      virtual void moveEvent(QMoveEvent *event);
      virtual void paintEvent(QPaintEvent* e);

      virtual void mouseReleaseEvent(QMouseEvent *event);
      virtual void mouseMoveEvent(QMouseEvent *event);
      virtual void leaveEvent(QEvent *event);
      virtual void wheelEvent(QWheelEvent *event);
  };
  typedef std::shared_ptr<RenderWindow> WindowRef;

} // namespace qtfullscreensystem


#endif /* QTF_WINDOW_HPP_ */
