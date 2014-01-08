/*
 * Window.cpp
 *
 *  Created on: Oct 29, 2013
 *      Author: tom
 */

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
  Window::Window(const QRect& geometry):
    _geometry(geometry),
    _img(nullptr)
  {
    qDebug() << "new window" << geometry;
    setGeometry(geometry);

    // don't allow user to change the window size
    setFixedSize(geometry.size());

    // transparent, always on top window without any decoration
    setWindowFlags( Qt::WindowStaysOnTopHint
                  | Qt::FramelessWindowHint
                  | Qt::MSWindowsOwnDC
                  //| Qt::X11BypassWindowManagerHint
                  );
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    setMask(QRegion(width() - 2, height() - 2, 1, 1));
    setWindowOpacity(0.5);
  }

  //----------------------------------------------------------------------------
  void Window::setImage(QImage const* img)
  {
    _img = img;
  }

  //----------------------------------------------------------------------------
  QBitmap createMaskFromColor( const QImage& img,
                               QRgb color,
                               const QRect& region )
  {
    if( img.depth() != 32 )
      return QBitmap();

    assert( img.depth() == 32 );
    assert( img.height() >= region.bottom() );
    assert( img.width()  >= region.right()  );

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

  //----------------------------------------------------------------------------
  void Window::moveEvent(QMoveEvent *event)
  {
    // X11: Window decorators add decoration after creating the window.
    // Win7: may not allow a window on top of the task bar
    //So we need to get the new frame of the window before drawing...
//    QPoint window_offset = mapToGlobal(QPoint());
//    QPoint window_end = mapToGlobal(QPoint(width(), height()));

    qDebug() << "move" << geometry();
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
  void Window::paintEvent(QPaintEvent* e)
  {
    QPainter painter(this);
    QRect reg = _geometry.translated(-QGuiApplication::primaryScreen()->availableVirtualGeometry().topLeft());
    //qDebug() << "paint" << geometry() << _geometry << reg;

    const QBitmap& mask = createMaskFromColor(*_img, qRgba(0,0,0, 0), reg);
    //mask.save(QString("mask-%1-%2.png").arg((uint64_t)this).arg(counter++));

    setMask(mask);
    painter.setBrush( QColor(255,127,127,127) );
    painter.setPen( QColor(127, 255, 127,64) );

    painter.drawRect(0,0,_geometry.width(),_geometry.height());
#if 0
    static size_t x = 0,
                  y = 0;

    x = (x + 6) % (_geometry.width() - 200);
    y = (x + 4) % (_geometry.height() - 200);

    QRect r1(x, y, 200, 200);
    painter.drawRect(r1);

    painter.setBrush( QColor(127,127,255,127) );
    QRect r2(_geometry.width() - x - 200,
                      _geometry.height() - y - 200,
                      200,
                      200);
    painter.drawRect(r2);
    painter.end();

//    QBitmap m(size());
//    painter.begin(&m);
//    painter.drawRect(r1);
//    painter.drawRect(r2);
//    painter.end();

    setMask( r1 );

    QTimer::singleShot(1000, this, SLOT(update()));
#endif
  }

  //----------------------------------------------------------------------------
  void Window::mouseMoveEvent(QMouseEvent *e)
  {
    qDebug() << "mouse" << e;
  }

} // namespace qtfullscreensystem
