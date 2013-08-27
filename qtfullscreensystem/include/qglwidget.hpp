#ifndef _GLWIDGET_HPP_
#define _GLWIDGET_HPP_

#include "render_thread.hpp"

#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/mouse_event.hpp"
#include "slotdata/text_popup.hpp"

#include "staticcore.h"
#include "xmlconfig.h"
#include "ipc_server.hpp"
#include "glcostanalysis.h"
#include "cpurouting.h"
#include "cpurouting-dijkstra.h"
#if USE_GPU_ROUTING
# include "gpurouting.h"
#endif
#include "glrenderer.h"

#include <QMutex>
#include <QGLWidget>
#include <QGLFramebufferObject>
#include <memory>

namespace qtfullscreensystem
{
  class GLWidget:
    public QWidget,
    public LinksRouting::Component,
    public LinksRouting::ComponentArguments
  {
    Q_OBJECT

    public:

      GLWidget(int& argc, char *argv[]);
      virtual ~GLWidget();

      /**
       * Signal that the screen is ready for being captured
       */
      void captureScreen();

      void publishSlots(LinksRouting::SlotCollector slots);
      void subscribeSlots(LinksRouting::SlotSubscriber& slot_subscriber);

      /**
       *
       */
      void setupGL();

      /**
       * Do actual render (To be called eg. by renderthread)
       */
      void render(int pass, QGLPixelBuffer* pbuffer);

      /**
       * Start rendering thread
       */
      void startRender();

      /**
       * Wait for new data (Used by render thread to wait for the next frame)
       */
      void waitForData();

      QImage _image;

    protected:

      virtual void paintEvent(QPaintEvent *event);
      virtual void resizeEvent(QResizeEvent *event);
      virtual void moveEvent(QMoveEvent *event);

      virtual void mouseReleaseEvent(QMouseEvent *event);
      virtual void mouseMoveEvent(QMouseEvent *event);
      virtual void leaveEvent(QEvent *event);
      virtual void wheelEvent(QWheelEvent *event);

      void updateScreenShot( QPoint window_offset,
                             QPoint window_end,
                             QGLPixelBuffer* pbuffer );

    protected slots:

      void onGlobalLinkShortcut();

    private:

      QSharedPointer<RenderThread> _render_thread;

      int _cur_fbo;
      std::unique_ptr<QGLFramebufferObject> _fbo_desktop[2];

      QPoint _window_offset;
      QPoint _window_end;

      bool _do_drag;
      float2 _last_mouse_pos;
      uint32_t _flags;

      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _slot_desktop;
      LinksRouting::slot_t<Rect>::type _slot_desktop_rect;
      LinksRouting::slot_t<LinksRouting::SlotType::MouseEvent>::type _slot_mouse;
      LinksRouting::slot_t<LinksRouting::SlotType::TextPopup>::type _slot_popups;
      QPixmap _screenshot;

      // TODO make readonly
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_links,
                                                                _subscribe_xray_fbo;
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_costmap;
      LinksRouting::slot_t<LinksRouting::LinkDescription::LinkList>::type _subscribe_routed_links;
      LinksRouting::slot_t<LinksRouting::SlotType::CoveredOutline>::type _subscribe_outlines;

      /** And now the components */
      LinksRouting::StaticCore      _core;
      LinksRouting::XmlConfig       _config,
                                    _user_config;
      LinksRouting::IPCServer       _server;
      LinksRouting::GlCostAnalysis  _cost_analysis;
      LinksRouting::CPURouting      _routing_cpu;
      LinksRouting::Dijkstra::CPURouting _routing_cpu_dijkstra;
#if USE_GPU_ROUTING
      LinksRouting::GPURouting      _routing_gpu;
#endif
      LinksRouting::GlRenderer      _renderer;

      QMutex            _mutex_slot_links;
      QWaitCondition    _cond_render;

      /** Use image from given file instead of desktop screenshot if not empty */
      std::string   _debug_desktop_image;

      /** Dump screenshot every n frames (Disabled if n == 0) */
      int           _dump_screenshot;
  };
} // namespace qtfullscreensystem

#endif /* _GLWIDGET_HPP_ */
