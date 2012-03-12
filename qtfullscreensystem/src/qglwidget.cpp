#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gl/glew.h>
#include <gl/gl.h>
#endif

#include "qglwidget.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <algorithm>

#include <GL/gl.h>


#include <QApplication>
#include <QBitmap>
#include <QDesktopWidget>
#include <QMoveEvent>

#include <QGLShader>

#define LOG_ENTER_FUNC() qDebug() << __FUNCTION__

namespace qtfullscreensystem
{

typedef QGLShaderProgram Shader;
typedef std::shared_ptr<Shader> ShaderPtr;

/**
 *
 * @param vert  Path to vertex shader
 * @param frag  Path to fragment shader
 */
ShaderPtr loadShader( QString vert, QString frag )
{
  qDebug() << "loadShader("<< vert << "|" << frag << ")";

  ShaderPtr program = std::make_shared<Shader>();

  if( !program->addShaderFromSourceFile(QGLShader::Vertex, vert) )
  {
    qWarning() << "Failed to load vertex shader (" << vert << ")\n"
               << program->log();
    return ShaderPtr();
  }

  if( !program->addShaderFromSourceFile(QGLShader::Fragment, frag) )
  {
    qWarning() << "Failed to load fragment shader (" << frag << ")\n"
               << program->log();
    return ShaderPtr();
  }

  if( !program->link() )
  {
    qWarning() << "Failed to link shader (" << vert << "|" << frag << ")\n"
               << program->log();
    return ShaderPtr();
  }

  return program;
}

  //----------------------------------------------------------------------------
  GLWidget::GLWidget(int& argc, char *argv[]):
    _render_thread(this)
  {
    //--------------------------------
    // Setup opengl and window
    //--------------------------------

    if( !QGLFormat::hasOpenGL() )
      qFatal("OpenGL not supported!");

    if( !QGLFramebufferObject::hasOpenGLFramebufferObjects() )
      qFatal("OpenGL framebufferobjects not supported!");

    // fullscreen
    setGeometry( QApplication::desktop()->screenGeometry(this) );

    // don't allow user to change the window size
    setFixedSize( size() );

    // transparent, always on top window without any decoration
    setWindowFlags( Qt::WindowStaysOnTopHint
                  | Qt::FramelessWindowHint
                  | Qt::MSWindowsOwnDC
                  //| Qt::X11BypassWindowManagerHint
                  );
    setWindowOpacity(0.5);
    setMask(QRegion(-1, -1, 1, 1));
    setAutoBufferSwap(false);

    if( !isValid() )
      qFatal("Unable to create OpenGL context (not valid)");

    qDebug
    (
      "Created GLWidget with OpenGL %d.%d",
      format().majorVersion(),
      format().minorVersion()
    );
    doneCurrent();

    //--------------------------------
    // Setup component system
    //--------------------------------

    if( argc != 2 )
      qFatal("Usage: %s <config>", argv[0]);

    assert(argc == 2 && argv && argv[1]);

    publishSlots(_core.getSlotCollector());

    _core.startup(argv[1]);
    _core.attachComponent(&_config);
    _core.attachComponent(&_server);
    _core.attachComponent(&_cost_analysis);
    _core.attachComponent(&_routing);
    _core.attachComponent(&_renderer);
    _core.init();

    _subscribe_links = _core.getSlotSubscriber().getSlot<LinksRouting::SlotType::Image>("/rendered-links");
    _subscribe_costmap = _core.getSlotSubscriber().getSlot<LinksRouting::SlotType::Image>("/costmap");
  }

  //----------------------------------------------------------------------------
  GLWidget::~GLWidget()
  {

  }

  //----------------------------------------------------------------------------
  void GLWidget::captureScreen()
  {
    LOG_ENTER_FUNC();

    _screenshot = QPixmap::grabWindow(QApplication::desktop()->winId());
  }

  //----------------------------------------------------------------------------
  void GLWidget::publishSlots(LinksRouting::SlotCollector slot_collector)
  {
    _slot_desktop =
      slot_collector.create<LinksRouting::SlotType::Image>("/desktop");
    _slot_desktop->_data->type = LinksRouting::SlotType::Image::OpenGLTexture;
  }

  //----------------------------------------------------------------------------
  ShaderPtr shader;
  void GLWidget::initGL()
  {
    LOG_ENTER_FUNC();

#if WIN32
    if(glewInit() != GLEW_OK)
      qFatal("Unable to init Glew");
#endif

    if( _fbo_desktop )
      qDebug("FBO already initialized!");

    _fbo_desktop.reset
    (
      new QGLFramebufferObject
      (
        size(),
        QGLFramebufferObject::Depth
      )
    );

    if( !_fbo_desktop->isValid() )
      qFatal("Failed to create framebufferobject!");

    _slot_desktop->_data->id = _fbo_desktop->texture();
    _slot_desktop->_data->width = size().width();
    _slot_desktop->_data->height = size().height();

    shader = loadShader("simple.vert", "remove_links.frag");
    if( !shader )
      qFatal("Failed to load shader.");

    glClearColor(0, 0, 0, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    _core.initGL();
  }

  //----------------------------------------------------------------------------
  void GLWidget::render()
  {
    // X11: Window decorators add decoration after creating the window.
    // Win7: may not allow a window on top of the task bar
    //So we need to get the new frame of the window before drawing...
    QPoint window_offset = mapToGlobal(QPoint());
    QPoint window_end = mapToGlobal(QPoint(width(), height()));
    LOG_ENTER_FUNC() << "window frame=" << window_offset <<" <-> " <<  window_end;

    _core.process();
    updateScreenShot(window_offset, window_end);

    // normal draw...
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,  _subscribe_links->_data->id);

    glBegin( GL_QUADS );

      glColor3f(1,1,1);

      glTexCoord2f(0,0);
      glVertex2f(-1,-1);

      glTexCoord2f(1,0);
      glVertex2f(1,-1);

      glTexCoord2f(1,1);
      glVertex2f(1,1);

      glTexCoord2f(0,1);
      glVertex2f(-1,1);

    glEnd();
    glDisable(GL_TEXTURE_2D);

    static int counter = 0;

    static QImage links(size(), QImage::Format_RGB888);
    glPushAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width(), height(), GL_RGB, GL_UNSIGNED_BYTE, links.bits());
    //links.mirrored().save(QString("links%1.png").arg(counter));

    //----------------------------------------------------------------------------
    auto writeTexture = []
    (
      const LinksRouting::slot_t<LinksRouting::SlotType::Image>::type& slot,
      const QString& name
    )
    {
      QImage image( QSize(slot->_data->width,
                          slot->_data->height),
                    QImage::Format_RGB888 );
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, slot->_data->id);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, image.bits());
      glDisable(GL_TEXTURE_2D);
      image.mirrored().save(name);
    };

    writeTexture(_subscribe_costmap, QString("costmap%1.png").arg(counter));
//    writeTexture(_slot_desktop, QString("desktop%1.png").arg(counter));
    writeTexture(_core.getSlotSubscriber().getSlot<LinksRouting::SlotType::Image>("/downsampled_desktop"), QString("downsampled_desktop%1.png").arg(counter));
    writeTexture(_core.getSlotSubscriber().getSlot<LinksRouting::SlotType::Image>("/featuremap"), QString("featuremap%1.png").arg(counter));

    glPopAttrib();

    //links.save("fbo.png");
    QBitmap mask = QBitmap::fromImage( links.mirrored().createMaskFromColor( qRgb(0,0,0) ) );
    //mask.save("mask.png");
    setMask(mask);


    ++counter;

    // TODO render flipped to not need mirror anymore
  }

  //----------------------------------------------------------------------------
  void GLWidget::startRender()
  {
    _render_thread.start();
  }

  //----------------------------------------------------------------------------
  void GLWidget::resizeEvent(QResizeEvent * event)
  {
    int w = event->size().width(),
        h = event->size().height();

    LOG_ENTER_FUNC() << "(" << w << "|" << h << ")"
                     << static_cast<float>(w)/h << ":" << 1;

    _render_thread.resize(w, h);
  }

  //----------------------------------------------------------------------------
  void GLWidget::moveEvent(QMoveEvent *event)
  {
    if( event->pos() != QPoint(0,0) )
      move( QPoint(0,0) );
  }

  //----------------------------------------------------------------------------
  void GLWidget::updateScreenShot(QPoint window_offset, QPoint window_end)
  {
    if( _screenshot.isNull() )
    {
      qWarning("No screenshot available...");
      return;
    }

    qDebug("Update screenshot...");
    _slot_desktop->setValid(false);

    // remove links from screenshot
    _fbo_desktop->bind();
    shader->bind();

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    bindTexture( _screenshot );

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _subscribe_links->_data->id);

    float w0x = static_cast<float>(window_offset.x()) / _screenshot.width(),
          w0y = 1- static_cast<float>(window_end.y()) / _screenshot.height();

    float w1x = static_cast<float>(window_end.x()) / _screenshot.width(),
          w1y = 1- static_cast<float>(window_offset.y()) / _screenshot.height();

    //y for opengl and windows are swapped
    //std::swap(w0y,w1y);

    shader->setUniformValue("desktop", 0);
    shader->setUniformValue("links", 1);

    glBegin( GL_QUADS );

      glMultiTexCoord2f(GL_TEXTURE0, 0,0);
      glMultiTexCoord2f(GL_TEXTURE1, w0x,w0y);
      glVertex2f(-1, -1);

      glMultiTexCoord2f(GL_TEXTURE0,1,0);
      glMultiTexCoord2f(GL_TEXTURE1,w1x,w0y);
      glVertex2f(1,-1);

      glMultiTexCoord2f(GL_TEXTURE0,1,1);
      glMultiTexCoord2f(GL_TEXTURE1,w1x,w1y);
      glVertex2f(1,1);

      glMultiTexCoord2f(GL_TEXTURE0,0,1);
      glMultiTexCoord2f(GL_TEXTURE1,w0x,w1y);
      glVertex2f(-1, 1);

    glEnd();

    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);

    shader->release();
    _fbo_desktop->release();
    _slot_desktop->setValid(true);

//    static int counter = 0;
    //if( !(counter++ % 5) )
//      _fbo_desktop->toImage().save( QString("desktop%1.png").arg(counter++) );

    _screenshot = QPixmap();
  }

} // namespace qtfullscreensystem
