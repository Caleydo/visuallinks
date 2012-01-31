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
  GLWidget::GLWidget(int& argc, char *argv[])
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

    if( !isValid() )
      qFatal("Unable to create OpenGL context (not valid)");

    qDebug
    (
      "Created GLWidget with OpenGL %d.%d",
      format().majorVersion(),
      format().minorVersion()
    );

    //--------------------------------
    // Setup component system
    //--------------------------------

    if( argc != 2 )
      qFatal("Usage: %s <config>", argv[0]);

    assert(argc == 2 && argv && argv[1]);

    publishSlots(_core.getSlotCollector());

    _core.startup(argv[1]);
    _core.attachComponent(&_config);
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

ShaderPtr shader;
//------------------------------------------------------------------------------
void GLWidget::initializeGL()
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

  shader = loadShader("simple.vert", "remove_links.frag");
  if( !shader )
    qFatal("Failed to load shader.");

  glClearColor(0, 0, 0, 0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  _core.initGL();
}

  //----------------------------------------------------------------------------
  void GLWidget::paintGL()
  {
    // X11: Window decorators add decoration after creating the window. So we
    // need to get the new position of the window before drawing...
    QPoint window_offset = mapToGlobal(QPoint());
    LOG_ENTER_FUNC() << "window offset=" << window_offset;

    _core.process();
    updateScreenShot(window_offset);

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

    static QImage links(size(), QImage::Format_RGB888);
    static QImage costmap(size(), QImage::Format_RGB888);
    static QImage desktop(size(), QImage::Format_RGB888);

    glPushAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width(), height(), GL_RGB, GL_UNSIGNED_BYTE, links.bits());

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _subscribe_costmap->_data->id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, costmap.bits());
    glDisable(GL_TEXTURE_2D);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _slot_desktop->_data->id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, desktop.bits());
    glDisable(GL_TEXTURE_2D);

    glPopAttrib();

    //links.save("fbo.png");
    QBitmap mask = QBitmap::fromImage( links.mirrored().createMaskFromColor( qRgb(0,0,0) ) );
    //mask.save("mask.png");
    setMask(mask);

    static int counter = 0;
    desktop.mirrored().save(QString("desktop%1.png").arg(counter));
    costmap.mirrored().save(QString("costmap%1.png").arg(counter));
    links.mirrored().save(QString("links%1.png").arg(counter));
    ++counter;

    // TODO render flipped to not need mirror anymore
  }

  //----------------------------------------------------------------------------
  void GLWidget::resizeGL(int w, int h)
  {
    LOG_ENTER_FUNC() << "(" << w << "|" << h << ")"
                     << static_cast<float>(w)/h << ":" << 1;

    glViewport(0,0,w,h);
    //glOrtho(0, static_cast<float>(w)/h, 0, 1, -1.0, 1.0);
  }

  //----------------------------------------------------------------------------
  void GLWidget::moveEvent(QMoveEvent *event)
  {
    if( event->pos() != QPoint(0,0) )
      move( QPoint(0,0) );
  }

  //----------------------------------------------------------------------------
  void GLWidget::updateScreenShot(QPoint window_offset)
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

    float dx = static_cast<float>(window_offset.x()) / width(),
          dy = static_cast<float>(window_offset.y()) / height();

    shader->setUniformValue("desktop", 0);
    shader->setUniformValue("links", 1);
    shader->setUniformValue("offset", dx, dy);

    glBegin( GL_QUADS );

      glTexCoord2f(dx,dy);
      glVertex2f(-1 + 2 * dx, -1 + 2 * dy);

      glTexCoord2f(1,dy);
      glVertex2f(1,-1 + 2 * dy);

      glTexCoord2f(1,1);
      glVertex2f(1,1);

      glTexCoord2f(dx,1);
      glVertex2f(-1 + 2 * dx, 1);

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
