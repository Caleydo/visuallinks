#ifndef _GLWIDGET_HPP_
#define _GLWIDGET_HPP_

#include "render_thread.hpp"

#include "slots.hpp"
#include "slotdata/image.hpp"

#include "staticcore.h"
#include "xmlconfig.h"
#include "ipc_server.hpp"
#include "glcostanalysis.h"
#include "gpurouting.h"
#include "glrenderer.h"

#include <QGLWidget>
#include <QGLFramebufferObject>
#include <memory>

namespace qtfullscreensystem
{
  // TODO make also component?
  class GLWidget: public QGLWidget
  {
    public:

      GLWidget(int& argc, char *argv[]);
      virtual ~GLWidget();

      /**
       * Signal that the screen is ready for being captured
       */
      void captureScreen();

      void publishSlots(LinksRouting::SlotCollector slots);

      /**
       *
       */
      void initGL();

      /**
       * Do actual render (To be called eg. by renderthread)
       */
      void render();

      /**
       * Start rendering thread
       */
      void startRender();

    protected:

      void resizeEvent(QResizeEvent * event);
      void moveEvent(QMoveEvent *event);

      void updateScreenShot(QPoint window_offset,  QPoint window_end);

    private:

      RenderThread  _render_thread;

      std::unique_ptr<QGLFramebufferObject> _fbo_desktop;

      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _slot_desktop;
      QPixmap _screenshot;

      // TODO make readonly
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_links;
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_costmap;

      /** And now the components */
      LinksRouting::StaticCore      _core;
      LinksRouting::XmlConfig       _config;
      LinksRouting::IPCServer       _server;
      LinksRouting::GlCostAnalysis  _cost_analysis;
      LinksRouting::GPURouting      _routing;
      LinksRouting::GlRenderer      _renderer;

  };
} // namespace qtfullscreensystem

#endif /* _GLWIDGET_HPP_ */
