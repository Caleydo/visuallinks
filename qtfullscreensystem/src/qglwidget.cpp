#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <qt_windows.h>
#include <gl/glew.h>
#include <gl/gl.h>
#endif

#include "qglwidget.hpp"
#include "log.hpp"

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

const QByteArray loadShaderSource( const QString& file_name,
                                      const QByteArray defs = "" )
{
  QFile file(file_name);
  if( !file.open(QFile::ReadOnly) )
  {
    qWarning() << "loadShaderSource: Unable to open file" << file_name;
    return QByteArray();
  }

  return defs + "\n" + file.readAll();
}

/**
 *
 * @param vert  Path to vertex shader
 * @param frag  Path to fragment shader
 */
ShaderPtr loadShader( QString vert, QString frag )
{
  qDebug() << "loadShader("<< vert << "|" << frag << ")";

  ShaderPtr program = std::make_shared<Shader>();
  QByteArray defs = "#define USE_DESKTOP_BLEND "
#ifdef USE_DESKTOP_BLEND
  "1"
#else
  "0"
#endif
  ;

  if( !program->addShaderFromSourceCode( QGLShader::Vertex,
                                         loadShaderSource(vert, defs) ) )
  {
    qWarning() << "Failed to load vertex shader (" << vert << ")\n"
               << program->log();
    return ShaderPtr();
  }

  if( !program->addShaderFromSourceCode( QGLShader::Fragment,
                                         loadShaderSource(frag, defs) ) )
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
    _render_thread(this),
    _cur_fbo(0),
    _server(&_mutex_slot_links)
  {
    //--------------------------------
    // Setup opengl and window
    //--------------------------------

    if( !QGLFormat::hasOpenGL() )
      qFatal("OpenGL not supported!");

    if( !QGLFramebufferObject::hasOpenGLFramebufferObjects() )
      qFatal("OpenGL framebufferobjects not supported!");

    // fullscreen
    setGeometry( QApplication::desktop()->screen(-1)->geometry() );

    // don't allow user to change the window size
    setFixedSize( size() );

    // transparent, always on top window without any decoration
    setWindowFlags( Qt::WindowStaysOnTopHint
                  | Qt::FramelessWindowHint
                  | Qt::MSWindowsOwnDC
                  //| Qt::X11BypassWindowManagerHint
                  );
#ifdef USE_DESKTOP_BLEND
    setAttribute(Qt::WA_TranslucentBackground);
#else
    setWindowOpacity(0.5);
#endif

#if defined(WIN32) || defined(_WIN32)
    setMask(QRegion(size().width() - 2, size().height() - 2, 1, 1));
#else
    setMask(QRegion(-1, -1, 1, 1));
#endif

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

    _core.attachComponent(this);
    registerArg("DebugDesktopImage", _debug_desktop_image);
    registerArg("DumpScreenshot", _dump_screenshot = 0);

    _core.init();

    LinksRouting::SlotSubscriber subscriber = _core.getSlotSubscriber();
    subscribeSlots(subscriber);

//    foreach (QWidget *widget, QApplication::allWidgets())
//    {
//      std::cout << widget->objectName().toStdString() << ": "
//                << widget->internalWinId() << " - "
//                << widget->windowTitle().toStdString() << " - "
//                << widget->window()->objectName().toStdString() << std::endl;
//    }

    std::cout << "Is virtual desktop: " << QApplication::desktop()->isVirtualDesktop() << std::endl;
  }

  //----------------------------------------------------------------------------
  GLWidget::~GLWidget()
  {

  }

  //----------------------------------------------------------------------------
  void GLWidget::captureScreen()
  {
    //LOG_ENTER_FUNC();

    QDesktopWidget *desktop = QApplication::desktop();
    _screenshot = QPixmap::grabWindow(
      desktop->winId(),
      0, 0,
      desktop->width(),
      desktop->height()
    );
  }

  //----------------------------------------------------------------------------
  void GLWidget::publishSlots(LinksRouting::SlotCollector slot_collector)
  {
    _slot_desktop =
      slot_collector.create<LinksRouting::SlotType::Image>("/desktop");
    _slot_desktop->_data->type = LinksRouting::SlotType::Image::OpenGLTexture;
  }

  //----------------------------------------------------------------------------
  void GLWidget::subscribeSlots(LinksRouting::SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/rendered-links");
    _subscribe_costmap =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/costmap");
    _subscribe_routed_links =
      slot_subscriber.getSlot<LinksRouting::LinkDescription::LinkList>("/links");
  }

  //----------------------------------------------------------------------------
  ShaderPtr shader, shader_blend;
  void GLWidget::setupGL()
  {
    LOG_ENTER_FUNC();

#if WIN32
    if(glewInit() != GLEW_OK)
      qFatal("Unable to init Glew");
#endif

    if( !_debug_desktop_image.empty() )
    {
      LOG_INFO("Using image instead of screenshot: " << _debug_desktop_image);
      if( _slot_desktop->isValid() )
        LOG_WARN("Already initialized!");

      static QPixmap img( QString::fromStdString(_debug_desktop_image) );

      _slot_desktop->_data->id = bindTexture(img);
      _slot_desktop->_data->width = img.width();
      _slot_desktop->_data->height = img.height();

      _slot_desktop->setValid(true);
    }
    else
    {

      if( _fbo_desktop[0] || _fbo_desktop[1] )
        qDebug("FBO already initialized!");

      _fbo_desktop[0].reset( new QGLFramebufferObject(size()) );
      _fbo_desktop[1].reset( new QGLFramebufferObject(size()) );

      if( !_fbo_desktop[0]->isValid() || !_fbo_desktop[1]->isValid() )
        qFatal("Failed to create framebufferobject!");

      _slot_desktop->_data->id = _fbo_desktop[_cur_fbo]->texture();
      _slot_desktop->_data->width = size().width();
      _slot_desktop->_data->height = size().height();

      shader = loadShader("simple.vert", "remove_links.frag");
      if( !shader )
        qFatal("Failed to load shader.");
    }

    shader_blend = loadShader("simple.vert", "blend.frag");
    if( !shader_blend )
      qFatal("Failed to load blend shader.");

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

    if(    window_offset != _window_offset
        || window_end != _window_end )
    {
      LOG_ENTER_FUNC() << "window frame="
                       << window_offset
                       <<" <-> "
                       <<  window_end;

      _window_offset = window_offset;
      _window_end = window_end;
    }

    updateScreenShot(window_offset, window_end);

    float x = _window_offset.x(),
          y = _window_offset.y(),
          w = _window_end.x() - x,
          h = _window_end.y() - y;

    glMatrixMode(GL_PROJECTION);
    glOrtho(x, x + w, y, y + h, -1.0, 1.0);


    {
      QMutexLocker lock_links(&_mutex_slot_links);
      _core.process();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // normal draw...
    //glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _subscribe_links->_data->id);

#ifndef USE_DESKTOP_BLEND
    shader_blend->bind();

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _slot_desktop->_data->id);

	shader_blend->setUniformValue("links", 0);
	shader_blend->setUniformValue("desktop", 1);

	glBegin( GL_QUADS );

      glMultiTexCoord2f(GL_TEXTURE0, 0,1);
      glVertex2f(-1, -1);

      glMultiTexCoord2f(GL_TEXTURE0,1,1);
      glVertex2f(1,-1);

      glMultiTexCoord2f(GL_TEXTURE0,1,0);
      glVertex2f(1,1);

      glMultiTexCoord2f(GL_TEXTURE0,0,0);
      glVertex2f(-1, 1);

    glEnd();

	glActiveTexture(GL_TEXTURE0);

	shader_blend->release();
#else
	glEnable(GL_TEXTURE_2D);
    glBegin( GL_QUADS );

      glColor3f(1,1,1);

      glTexCoord2f(0,1);
      glVertex2f(-1,-1);

      glTexCoord2f(1,1);
      glVertex2f(1,-1);

      glTexCoord2f(1,0);
      glVertex2f(1,1);

      glTexCoord2f(0,0);
      glVertex2f(-1,1);

    glEnd();
    glDisable(GL_TEXTURE_2D);
#endif

    glBindTexture(GL_TEXTURE_2D, 0);
    static int counter = 0;

    static QImage links(size(), QImage::Format_RGB888);
    glPushAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width(), height(), GL_RGB, GL_UNSIGNED_BYTE, links.bits());
    //links.mirrored().save(QString("links%1.png").arg(counter));

    //--------------------------------------------------------------------------
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
      image.save(name);
    };

//    writeTexture(_subscribe_costmap, QString("costmap%1.png").arg(counter));
    if( _dump_screenshot && !(counter % _dump_screenshot) )
      writeTexture(_slot_desktop, QString("desktop%1.png").arg(counter));
//    writeTexture(_core.getSlotSubscriber().getSlot<LinksRouting::SlotType::Image>("/downsampled_desktop"), QString("downsampled_desktop%1.png").arg(counter));
//    writeTexture(_core.getSlotSubscriber().getSlot<LinksRouting::SlotType::Image>("/featuremap"), QString("featuremap%1.png").arg(counter));

    glPopAttrib();

    //links.save("fbo.png");
    if( !_subscribe_routed_links->_data->empty() )
    {
//      writeTexture(_subscribe_links, QString("links%1.png").arg(counter));

      QBitmap mask = QBitmap::fromImage( links.mirrored().createMaskFromColor( qRgb(0,0,0) ) );
      setMask(mask);
//      mask.save(QString("mask%1.png").arg(counter));
    }
    else
#if defined(WIN32) || defined(_WIN32)
      setMask(QRegion(size().width() - 2, size().height() - 2, 1, 1));
#else
      setMask(QRegion(-1, -1, 1, 1));
#endif

    ++counter;
	/*
	QPixmap screen = QPixmap::fromImage( grabFrameBuffer(true) );
	screen.save(QString("links%1.png").arg(counter));*/

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
    if( _screenshot.isNull() || !_fbo_desktop[0] || !_fbo_desktop[1] )
      return;

    //qDebug("Update screenshot...");

    _slot_desktop->setValid(false);
    _cur_fbo = 1 - _cur_fbo;

    // remove links from screenshot
    _fbo_desktop[_cur_fbo]->bind();
    shader->bind();

    glActiveTexture(GL_TEXTURE0);
    GLuint tid = bindTexture(_screenshot, GL_TEXTURE_2D, GL_RGBA, QGLContext::NoBindOption);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _subscribe_links->_data->id);

    shader->setUniformValue("screenshot", 0);
    shader->setUniformValue("links", 1);

#ifndef USE_DESKTOP_BLEND
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, _slot_desktop->_data->id);

    shader->setUniformValue("desktop", 2);
#endif

    float w0x = static_cast<float>(window_offset.x()) / _screenshot.width(),
          w0y = static_cast<float>(window_offset.y()) / _screenshot.height();

    float w1x = static_cast<float>(window_end.x()) / _screenshot.width(),
          w1y = static_cast<float>(window_end.y()) / _screenshot.height();

    //y for opengl and windows are swapped
    //std::swap(w0y,w1y);

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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);

    shader->release();
    _fbo_desktop[_cur_fbo]->release();

    _slot_desktop->_data->id = _fbo_desktop[_cur_fbo]->texture();
    _slot_desktop->setValid(true);

//    static int counter = 0;
    //if( !(counter++ % 5) )
//      _fbo_desktop->toImage().save( QString("desktop%1.png").arg(counter++) );

    _screenshot = QPixmap();
  }

} // namespace qtfullscreensystem
