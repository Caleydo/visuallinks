#ifndef _GLWIDGET_HPP_
#define _GLWIDGET_HPP_

#include "render_thread.hpp"

#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/mouse_event.hpp"

#include "staticcore.h"
#include "xmlconfig.h"
#include "ipc_server.hpp"
#include "glcostanalysis.h"
#include "cpurouting.h"
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

      virtual void mouseMoveEvent(QMouseEvent *event);
      virtual void leaveEvent(QEvent *event);

      void updateScreenShot( QPoint window_offset,
                             QPoint window_end,
                             QGLPixelBuffer* pbuffer );

    private:

      QSharedPointer<RenderThread> _render_thread;

      int _cur_fbo;
      std::unique_ptr<QGLFramebufferObject> _fbo_desktop[2];

      QPoint _window_offset;
      QPoint _window_end;

      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _slot_desktop;
      LinksRouting::slot_t<LinksRouting::SlotType::MouseEvent>::type _slot_mouse;
      QPixmap _screenshot;

      // TODO make readonly
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_links;
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_costmap;
      LinksRouting::slot_t<LinksRouting::LinkDescription::LinkList>::type _subscribe_routed_links;

      /** And now the components */
      LinksRouting::StaticCore      _core;
      LinksRouting::XmlConfig       _config,
                                    _user_config;
      LinksRouting::IPCServer       _server;
      LinksRouting::GlCostAnalysis  _cost_analysis;
      LinksRouting::CPURouting      _routing_cpu;
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
