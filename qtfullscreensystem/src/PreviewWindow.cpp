/*
 * PreviewWindow.cpp
 *
 *  Created on: Oct 29, 2013
 *      Author: tom
 */

// Include first to get conversion functions to/from float2
#include <QPoint>
#include <QRect>
#include <QSize>

#include "PreviewWindow.hpp"
#include <NodeRenderer.hpp>

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
  const int _preview_width = 700; // TODO
  const int _preview_height = 400; // TODO

  //----------------------------------------------------------------------------
  QtPreviewWindow::QtPreviewWindow(Popup *popup):
    PreviewWindow(popup)
  {
    init();
  }

  //----------------------------------------------------------------------------
  QtPreviewWindow::QtPreviewWindow(SeeThrough *see_through):
    PreviewWindow(see_through)
  {
    init();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::subscribeSlots(LR::SlotSubscriber& slot_subscriber)
  {
    _subscribe_mouse =
      slot_subscriber.getSlot<LR::SlotType::MouseEvent>("/mouse");
    _subscribe_popups =
      slot_subscriber.getSlot<LR::SlotType::TextPopup>("/popups");
    _subscribe_tile_handler =
      slot_subscriber.getSlot<LR::SlotType::TileHandler>("/tile-handler");
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::initialize()
  {

  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::setPreviewGeometry(QRect const& reg)
  {
    _geometry = reg;
    setGeometry( reg.adjusted(-_margin, -_margin, _margin, _margin) );
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::render(QPainter*)
  {
    if( _popup )            renderPopup();
    else if( _see_through ) renderSeeThrough();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::render()
  {
    updateGeometry();

    if( !_device )
      _device = new QOpenGLPaintDevice;

    glClearColor(.3f, .3f, .3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    int const w = width() - 2 * _margin,
              h = height() - 2 * _margin;
    glViewport(_margin, _margin, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    _device->setSize(size());

    QPainter painter(_device);
    render(&painter);
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::update(double)
  {
    if( !_popup )
      return;

    setOpacity(_popup->hover_region.getAlpha());

    auto tile_map = _popup->hover_region.tile_map.lock();
    if( tile_map && _tile_map_change_id != tile_map->getChangeId() )
    {
      renderLater();
      _tile_map_change_id = tile_map->getChangeId();
    }
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::release()
  {
    qDebug() << "release";
    hide();
    deleteLater();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::renderLater()
  {
    if( !_update_pending )
    {
      _update_pending = true;
      QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::renderNow()
  {
    if( !isExposed() )
      return;

    bool needs_init = false;

    if( !_context )
    {
      _context = new QOpenGLContext(this);
      _context->setFormat(requestedFormat());
      _context->create();

      needs_init = true;
    }

    _context->makeCurrent(this);

    if( needs_init )
    {
      initializeOpenGLFunctions();
      initialize();
    }

    render();

    _context->swapBuffers(this);

//    if( _animating )
//      renderLater();
  }

  //----------------------------------------------------------------------------
  bool QtPreviewWindow::event(QEvent *event)
  {
    switch( event->type() )
    {
      case QEvent::UpdateRequest:
        _update_pending = false;
        renderNow();
        return true;
      case QEvent::Leave:
        std::cout << "leave " << _do_drag << std::endl;
        if( _do_drag )
          return true;

        if( _popup )
          _popup->hover_region.delayedFadeOut();
        if( _see_through )
          _see_through->delayedFadeOut();

        return true;
      default:
        return QWindow::event(event);
    }
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::exposeEvent(QExposeEvent*)
  {
    if( isExposed() )
      renderNow();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::mouseReleaseEvent(QMouseEvent *event)
  {
    if( _do_drag )
      _do_drag = false;
    else
      _subscribe_mouse->_data->triggerClick(event->globalX(), event->globalY());
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::mouseMoveEvent(QMouseEvent *event)
  {
    if( _popup )
      _popup->hover_region.show();

//    auto& mouse = *_subscribe_mouse->_data;
//    qDebug() << "mouse" << event;
//
    if( !event->buttons() )
    {
      //qDebug() << "move" << event->globalX() << event->globalY();
    }
    else if( event->buttons() & Qt::LeftButton )
    {
      float2 pos(event->globalX(), event->globalY());
      if( !_do_drag )
        _do_drag = true;
      else
      {
        if( _popup )
        {
          float preview_aspect = _preview_width
                               / static_cast<float>(_preview_height);

          float2 delta = pos - _last_mouse_pos;
          const float2& scroll_size = _popup->hover_region.scroll_region.size;
          float step_y = scroll_size.y
                       / pow(2.0f, static_cast<float>(_popup->hover_region.zoom))
                       / _preview_height;

          _popup->hover_region.src_region.pos.y -= delta.y * step_y;
          float step_x = step_y * preview_aspect;
          _popup->hover_region.src_region.pos.x -= delta.x * step_x;

          _subscribe_tile_handler->_data->updateRegion(*_popup);
          renderLater();
        }
      }
      _last_mouse_pos = pos;
    }

    renderLater();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::wheelEvent(QWheelEvent *event)
  {
    _subscribe_mouse->_data->triggerScroll(
      event->delta(),
      float2(event->globalX(), event->globalY()),
      event->modifiers()
    );

    renderLater();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::renderPopup()
  {
    LR::SlotType::TextPopup::HoverRect const& preview = _popup->hover_region;

    if( !preview.isVisible() )
      return;

    std::cout << "render preview:" << std::endl;
    auto tile_map = preview.tile_map.lock();
    if( tile_map )
      tile_map->render( preview.src_region,
                        preview.scroll_region.size,
                        Rect(float2(0, 0), size()),
                        _popup->hover_region.zoom,
                        _popup->auto_resize,
                        _popup->hover_region.getAlpha(),
                        &_image_cache );

//    glMatrixMode(GL_PROJECTION);
//    glPushMatrix();
//    glLoadIdentity();
//    glOrtho(0, hover.size.x, 0, hover.size.y, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const Rect& hover = preview.region,
              & src = preview.src_region,
              & scroll = preview.scroll_region;

    float scale = hover.size.y / src.size.y;
    glScalef(scale, scale, 0);

    float2 offset = src.pos;
    if( !_popup->auto_resize && src.size.x > scroll.size.x )
      offset.x -= (src.size.x - scroll.size.x) / 2;

    glTranslatef(-offset.x, -offset.y, 0);

    const float w = 4.f;
    const float2 vp_pos = preview.offset + 0.5f * float2(w, w);
    const float2 vp_dim = preview.dim - 4.f * float2(w, w);

    Partitions *partitions_src = nullptr,
               *partitions_dest = nullptr;
    unsigned int margin_left = 0;

    if( tile_map )
    {
      partitions_src = &tile_map->partitions_src;
      partitions_dest = &tile_map->partitions_dest;
      margin_left = tile_map->margin_left;
    }

    LinksRouting::NodeRenderer renderer( partitions_src,
                                         partitions_dest,
                                         margin_left );
    renderer.setLineWidth(3.f / scale);

//    Color color_cur = _color_cur;
//    _color_cur = alpha * color_cur;
    renderer.renderNodes(_popup->nodes, nullptr, nullptr, true, 1, false);

    glLineWidth(w);
    glBegin(GL_LINE_LOOP);
      glVertex2f(vp_pos.x, vp_pos.y);
      glVertex2f(vp_pos.x + vp_dim.x, vp_pos.y);
      glVertex2f(vp_pos.x + vp_dim.x, vp_pos.y + vp_dim.y);
      glVertex2f(vp_pos.x, vp_pos.y + vp_dim.y);
    glEnd();

//    _color_cur = color_cur;

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);

//    glPopAttrib();
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::renderSeeThrough()
  {
    HierarchicTileMapPtr tile_map = _see_through->tile_map.lock();
    if( !tile_map )
    {
      std::cout << "SeeThrough tile_map expired!" << std::endl;
      return;
    }

    std::cout << "render tilemap: " << _see_through->source_region << std::endl;

    //      renderRect( preview.preview_region );
    tile_map->render( _see_through->source_region,
                      _see_through->source_region.size,
                      Rect(float2(0, 0), _see_through->preview_region.size),
                      -1,
                      false,
                      1,
                      &_image_cache );
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::init()
  {
    _margin = 2;
    _do_drag = false;
    _last_mouse_pos = float2(0,0);
    _tile_map_change_id = 0;
    _update_pending = false;
    _context = nullptr;
    _device = nullptr;

    setSurfaceType(QWindow::OpenGLSurface);
    updateGeometry();

    // transparent, always on top window without any decoration
    setFlags( Qt::WindowStaysOnTopHint
            | Qt::FramelessWindowHint
            | Qt::X11BypassWindowManagerHint
            //| Qt::WindowTransparentForInput
            );
    //setMouseTracking(true);
    setOpacity(0.5);
    show();

    qDebug() << "new preview" << this->geometry();

    HierarchicTileMapPtr tile_map;

    if( _popup )
      tile_map = _popup->hover_region.tile_map.lock();
    else if( _see_through )
      tile_map = _see_through->tile_map.lock();

    if( tile_map )
      tile_map->addTileChangeCallback(
        std::bind<>(&QtPreviewWindow::onTileMapChange, this)
      );
    else
      std::cout << "missing tilemap" << std::endl;
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::updateGeometry()
  {
    Rect geom;
    if( _popup )
      geom = _popup->hover_region.region;
    else if( _see_through )
      geom = _see_through->preview_region;

    if( geom.toQRect() == _geometry )
      return;

    setPreviewGeometry(geom.toQRect());
    //    setFixedSize(size());
  }

  //----------------------------------------------------------------------------
  void QtPreviewWindow::onTileMapChange()
  {
    std::cout << "tilemap changed" << std::endl;
    renderLater();
  }

} // namespace qtfullscreensystem
