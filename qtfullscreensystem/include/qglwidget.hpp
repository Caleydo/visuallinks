#ifndef _GLWIDGET_HPP_
#define _GLWIDGET_HPP_

#include "slots.hpp"
#include "slotdata/image.hpp"

#include "staticcore.h"
#include "xmlconfig.h"
#include "glcostanalysis.h"
#include "cpurouting.h"
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

    protected:

      // QT OpenGL callbacks
      virtual void initializeGL();
      virtual void paintGL();
      virtual void resizeGL(int width, int height);

      virtual void moveEvent(QMoveEvent *event);

      void updateScreenShot(QPoint window_offset);

    private:

      std::unique_ptr<QGLFramebufferObject> _fbo_desktop;

      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _slot_desktop;
      QPixmap _screenshot;

      // TODO make readonly
      LinksRouting::slot_t<LinksRouting::SlotType::Image>::type _subscribe_links;

      /** And now the components */
      LinksRouting::StaticCore  _core;
      LinksRouting::XmlConfig   _config;
      LinksRouting::GlRenderer  _renderer;

  };
} // namespace qtfullscreensystem

#endif /* _GLWIDGET_HPP_ */
