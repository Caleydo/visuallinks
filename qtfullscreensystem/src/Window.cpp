/*
 * RenderWindow.cpp
 *
 *  Created on: Oct 29, 2013
 *      Author: tom
 */

// Include first to get conversion functions to/from float2
#include <QPoint>
#include <QRect>
#include <QSize>

#include "Window.hpp"
#include <QBitmap>
#include <QDebug>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>

#include <cassert>
#include <iostream>

namespace qtfullscreensystem
{
  //----------------------------------------------------------------------------
  RenderWindow::RenderWindow(const QRect& geometry):
    _geometry(geometry),
    _img(nullptr),
    _do_drag(false),
    _last_mouse_pos(0,0)
  {
    qDebug() << "new window" << geometry;
    setGeometry(geometry);

    // don't allow user to change the window size
    setFixedSize(geometry.size());

    // transparent, always on top window without any decoration
    setWindowFlags( Qt::WindowStaysOnTopHint
                  | Qt::FramelessWindowHint
                  | Qt::MSWindowsOwnDC
                  | Qt::X11BypassWindowManagerHint
                  //| Qt::WindowTransparentForInput
                  );
    //setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    //setAttribute(Qt::WA_TransparentForMouseEvents);
    //setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    setMask(QRegion(width() - 2, height() - 2, 1, 1));
    //setWindowOpacity(0.5);
  }

  //----------------------------------------------------------------------------
  void RenderWindow::subscribeSlots(LR::SlotSubscriber& slot_subscriber)
  {
    _subscribe_mouse =
      slot_subscriber.getSlot<LR::SlotType::MouseEvent>("/mouse");
    _subscribe_popups =
      slot_subscriber.getSlot<LR::SlotType::TextPopup>("/popups");
    _subscribe_outlines =
      slot_subscriber.getSlot<LR::SlotType::CoveredOutline>("/covered-outlines");
    _subscribe_xray =
      slot_subscriber.getSlot<LR::SlotType::XRayPopup>("/xray");
  }

  //----------------------------------------------------------------------------
  void RenderWindow::setImage(QImage const* img)
  {
    _img = img;

    setMouseTracking(false);
    setWindowFlags( windowFlags()
                  | Qt::WindowTransparentForInput
                  );
    setAttribute(Qt::WA_TransparentForMouseEvents);

    setMask(QRegion(0, 0, width(), height()));
  }

  //----------------------------------------------------------------------------
  QBitmap createMaskFromColor( const QImage& img,
                               QRgb color,
                               const QRect& region )
  {
    QImage maskImage(region.size(), QImage::Format_MonoLSB);
    maskImage.fill(0);
    uchar *s = maskImage.bits();

    for(int h = region.top(); h < region.bottom(); h++)
    {
      const uint *sl = (uint *)img.scanLine(h);
      for(int w = region.left(); w < region.right(); w++)
      {
        if( sl[w] == color )
          *(s + ((w - region.left()) >> 3)) |= (1 << ((w - region.left()) & 7));
      }
      s += maskImage.bytesPerLine();
    }

    return QBitmap::fromImage(maskImage);
  }

  void maskFillRect( QImage& mask_img,
                     const QRect& rect )
  {
    int b = std::min(rect.bottom(), mask_img.height()),
        r = std::min(rect.right(), mask_img.width());

    uchar *s = mask_img.scanLine(rect.top());
    for(int h = rect.top(); h < b; ++h)
    {
      for(int w = rect.left(); w < r; w++)
        // TODO optimize: fill contiguous regions at once...
        *(s + (w >> 3)) &= ~(1 << (w & 7));
      s += mask_img.bytesPerLine();
    }
  }

  //----------------------------------------------------------------------------
  void RenderWindow::moveEvent(QMoveEvent *event)
  {
    // X11: RenderWindow decorators add decoration after creating the window.
    // Win7: may not allow a window on top of the task bar
    //So we need to get the new frame of the window before drawing...
//    QPoint window_offset = mapToGlobal(QPoint());
//    QPoint window_end = mapToGlobal(QPoint(width(), height()));

    _geometry.setTopLeft(pos());

//    if(    window_offset != _window_offset
//        || window_end != _window_end )
//    {
//      LOG_ENTER_FUNC() << "window frame="
//                       << window_offset
//                       <<" <-> "
//                       <<  window_end;
//
//      _window_offset = window_offset;
//      _window_end = window_end;
//
//      // Publish desktop region which contains only drawable areas
//      *_slot_desktop_rect->_data =
//        QRect(window_offset, window_end - window_offset - QPoint(1,1));
//
//      _slot_desktop_rect->setValid(true);
//
//      // Moves the window to the top- and leftmost position. As long as the
//      // offset changes either the user is moving the window or the window is
//      // being created.
//      move( QPoint(49,24) );
//    }
  }

  //----------------------------------------------------------------------------
  void RenderWindow::paintEvent(QPaintEvent* e)
  {
    QRect reg = _geometry.translated(
      -QGuiApplication::primaryScreen()->availableVirtualGeometry().topLeft()
    );
    QRect local_reg(QPoint(0,0), size());

    QImage mask_img;
    bool mask_empty = true;
    if( _img )
    {
      if( _img->isNull() )
        return;

      assert( _img->depth() == 32 );
      assert( _img->height() >= reg.bottom() );
      assert( _img->width()  >= reg.right()  );
    }
    else // TODO add switch to create full window mask?
    {
      mask_img = QImage(reg.size(), QImage::Format_MonoLSB);
      mask_img.fill(1);
    }

    QPainter painter(this);
//    qDebug() << "paint" << geometry() << _geometry << reg;

    // TODO const QBitmap& mask = createMaskFromColor(*_img, qRgba(0,0,0, 0), reg);
    //mask.save(QString("mask-%1-%2.png").arg((uint64_t)this).arg(counter++));

    //setMask(mask);
    if( _img )
      painter.drawImage( QRectF(0, 0, -1, -1),
                         *_img,
                         QRectF(reg) );

    for( auto popup = _subscribe_popups->_data->popups.begin();
              popup != _subscribe_popups->_data->popups.end();
            ++popup )
    {
      float2 pos = popup->region.region.pos
                 - float2(_geometry.x(), _geometry.y() + 1);

      if(    pos.x < 0 || pos.x > _geometry.width()
          || pos.y < 0 || pos.y > _geometry.height()
          || !popup->region.isVisible()
          || (popup->node && popup->node->get<bool>("hidden")) )
        continue;

      QRect text_rect(pos.toQPoint(), popup->region.region.size.toQSize());

      painter.drawText(
        text_rect,
        Qt::AlignCenter,
        QString::fromStdString(popup->text)
      );

      if( !mask_img.isNull() )
      {
        maskFillRect(mask_img, text_rect);
        mask_empty = false;
      }
    }

    painter.setPen(Qt::white);
    for( auto const& outline: _subscribe_outlines->_data->popups )
    {
      float2 pos = outline.region_title.pos
                 - float2(_geometry.x() - 5, _geometry.y() + 4);

      if(  pos.x < 0 || pos.x > _geometry.width()
        || pos.y < 0 || pos.y > _geometry.height() )
        continue;

      QRect text_rect(pos.toQPoint(), outline.region_title.size.toQSize());

      size_t len = outline.title.size();
      if( !outline.preview->isVisible() )
        len = std::min<size_t>(19, len);
      QString title = QString::fromLatin1(outline.title.data(), (int)len);
      if( len < outline.title.size() )
        title += "...";

      painter.drawText(
        text_rect,
        Qt::AlignLeft | Qt::AlignVCenter,
        title
      );

      if( !mask_img.isNull() && text_rect.intersects(local_reg) )
      {
        maskFillRect(mask_img, text_rect);
        mask_empty = false;
      }
    }

    if( !mask_img.isNull() )
    {
      for( auto const& popup: _subscribe_popups->_data->popups )
      {
        if( !popup.hover_region.isVisible() )
          continue;

        QRect popup_rect( popup.hover_region.region.pos.toQPoint()
                        - _geometry.topLeft(),
                          popup.hover_region.region.size.toQSize() );

        if( !popup_rect.intersects(local_reg) )
          continue;

        maskFillRect(mask_img, popup_rect);
        mask_empty = false;
      }

      for(auto const& preview: _subscribe_xray->_data->popups)
      {
        if( !preview.isVisible() || preview.tile_map.expired() )
          continue;

        QRect reg = preview.preview_region.toQRect()
                                          .translated(-_geometry.topLeft());
        if( !reg.intersects(local_reg) )
          continue;

        maskFillRect(mask_img, reg);
        mask_empty = false;
      }

      if( mask_empty )
        setMask(QRegion(width() - 2, height() - 2, 1, 1));
      else
        setMask(QBitmap::fromImage(mask_img));
//      static size_t counter = 0;
//      QBitmap::fromImage(mask_img).save(QString("mask-%1-%2.png").arg((uint64_t)this).arg(counter++));
    }
  }

  //----------------------------------------------------------------------------
  void RenderWindow::mouseReleaseEvent(QMouseEvent *event)
  {
    if( _do_drag )
      _do_drag = false;
    else
      _subscribe_mouse->_data->triggerClick(event->globalX(), event->globalY());
  }

  //----------------------------------------------------------------------------
  void RenderWindow::mouseMoveEvent(QMouseEvent *event)
  {
    auto& mouse = *_subscribe_mouse->_data;
    qDebug() << "mouse" << event;

    if( !event->buttons() )
      mouse.triggerMove(event->globalX(), event->globalY());
    else if( event->buttons() & Qt::LeftButton )
    {
      float2 pos(event->globalX(), event->globalY());
      if( _do_drag )
        mouse.triggerDrag(pos - _last_mouse_pos);
      else
        _do_drag = true;
      _last_mouse_pos = pos;
    }
  }

  //----------------------------------------------------------------------------
  void RenderWindow::leaveEvent(QEvent *event)
  {
    _subscribe_mouse->_data->triggerLeave();
  }

  //----------------------------------------------------------------------------
  void RenderWindow::wheelEvent(QWheelEvent *event)
  {
    _subscribe_mouse->_data->triggerScroll(
      event->delta(),
      float2(event->globalX(), event->globalY()),
      event->modifiers()
    );
  }

} // namespace qtfullscreensystem
