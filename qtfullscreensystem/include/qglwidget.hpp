#ifndef _GLWIDGET_HPP_
#define _GLWIDGET_HPP_

#include "render_thread.hpp"

#include "slots.hpp"
#include "slotdata/image.hpp"

#include "staticcore.h"
#include "xmlconfig.h"
#include "ipc_server.hpp"
#include "glcostanalysis.h"
#include "cpurouting.h"
#include "gpurouting.h"
#include "glrenderer.h"

#include <QMutex>
#include <QGLWidget>
#include <QGLFramebufferObject>
#include <memory>

namespace qtfullscreensystem
{
  class GLWidget:
    public QWidget,
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

      const std::string& name() const
      {
        static const std::string name("QGLWidget");
        return name;
      }

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

      void paintEvent(QPaintEvent *event);
      void resizeEvent(QResizeEvent * event);
      void moveEvent(QMoveEvent *event);

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
      QPixmap _screenshot;

      // TODO make readonly
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_links;
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_costmap;
      LinksRouting::slot_t<LinksRouting::LinkDescription::LinkList>::type _subscribe_routed_links;

      /** And now the components */
      LinksRouting::StaticCore      _core;
      LinksRouting::XmlConfig       _config;
      LinksRouting::IPCServer       _server;
      LinksRouting::GlCostAnalysis  _cost_analysis;
      LinksRouting::CPURouting      _routing_cpu;
      LinksRouting::GPURouting      _routing_gpu;
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
