#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <qt_windows.h>
#include <gl/glew.h>
#include <gl/gl.h>
#endif

#include <QRect>
#include "qglwidget.hpp"
#include "clock.hxx"
#include "log.hpp"
#include "qt_helper.hxx"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <algorithm>

#include <GL/gl.h>

#include <QApplication>
#include <QBitmap>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QMoveEvent>
#include <QStaticText>

#include <QGLShader>
#include <QGLPixelBuffer>

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
    Configurable("QGLWidget"),
    _cur_fbo(0),
    _do_drag(false),
    _server(&_mutex_slot_links, &_cond_render, this)
  {
    QApplication::setOrganizationDomain("icg.tugraz.at");
    QApplication::setOrganizationName("icg.tugraz.at");
    QApplication::setApplicationName("VisLinks");

    const QDir config_dir =
      QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    const QString user_config = config_dir.filePath("config.xml");

    {
      // Ensure config path and user config file exist
      QDir().mkpath(config_dir.path());
      QFile file( user_config );
      if( !file.open(QIODevice::ReadWrite | QIODevice::Text) )
        LOG_ERROR("Failed to open user config!");
      else if( !file.size() )
      {
        LOG_INFO("Creating empty user config in '" << user_config << "'");
        file.write(
          "<!-- VisLinks user config (overrides config file) -->\n"
          "<config/>"
        );
      }
    }

    //--------------------------------
    // Setup opengl and window
    //--------------------------------

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

    setMouseTracking(true);

    //--------------------------------
    // Setup component system
    //--------------------------------

    if( argc != 2 )
      qFatal("Usage: %s <config>", argv[0]);

    assert(argc == 2 && argv && argv[1]);

    publishSlots(_core.getSlotCollector());

    _config.initFrom(argv[1]);
    _user_config.initFrom( to_string(user_config) );

    _core.startup();
    _core.attachComponent(&_config);
    _core.attachComponent(&_user_config);
    _core.attachComponent(&_server);
#ifdef USE_GPU_ROUTING
    _core.attachComponent(&_cost_analysis);
#endif
    _core.attachComponent(&_routing_cpu);
#ifdef USE_GPU_ROUTING
    _core.attachComponent(&_routing_gpu);
#endif
    _core.attachComponent(&_renderer);

    _core.attachComponent(this);
    registerArg("DebugDesktopImage", _debug_desktop_image);
    registerArg("DumpScreenshot", _dump_screenshot = 0);

    _core.init();

    LinksRouting::SlotSubscriber subscriber = _core.getSlotSubscriber();
    subscribeSlots(subscriber);

    _render_thread =
      QSharedPointer<RenderThread>(new RenderThread(this, width(), height()));

    //std::cout << "Is virtual desktop: "
    //          << QApplication::desktop()->isVirtualDesktop()
    //          << std::endl;
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

    _slot_mouse =
      slot_collector.create<LinksRouting::SlotType::MouseEvent>("/mouse");
    _slot_popups =
      slot_collector.create<LinksRouting::SlotType::TextPopup>("/popups");
  }

  //----------------------------------------------------------------------------
  void GLWidget::subscribeSlots(LinksRouting::SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/rendered-links");
    _subscribe_xray =
      slot_subscriber.getSlot<LinksRouting::SlotType::XRayPopup>("/x-ray");
#ifdef USE_GPU_ROUTING
    _subscribe_costmap =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/costmap");
#endif
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
//      LOG_INFO("Using image instead of screenshot: " << _debug_desktop_image);
//      if( _slot_desktop->isValid() )
//        LOG_WARN("Already initialized!");
//
//      static QPixmap img( QString::fromStdString(_debug_desktop_image) );
//
//      _slot_desktop->_data->id = bindTexture(img);
//      _slot_desktop->_data->width = img.width();
//      _slot_desktop->_data->height = img.height();
//
//      _slot_desktop->setValid(true);
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
  void GLWidget::render(int pass, QGLPixelBuffer* pbuffer)
  {
    PROFILE_START()

#ifdef USE_GPU_ROUTING
    if( pass == 0 )
    {
      updateScreenShot(_window_offset, _window_end, pbuffer);
      PROFILE_RESULT("  updateScreenshot")
    }
#endif

    float x = _window_offset.x(),
          y = _window_offset.y(),
          w = _window_end.x() - x,
          h = _window_end.y() - y;

    glMatrixMode(GL_PROJECTION);
    glOrtho(x, x + w, y, y + h, -1.0, 1.0);

    {
      QMutexLocker lock_links(&_mutex_slot_links);
      _core.process(pass == 0 ? (Component::Renderer | 64) : Component::Any);
    }

    PROFILE_RESULT("  core")

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // normal draw...
    glClear(GL_COLOR_BUFFER_BIT);
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

//    static QImage links(size(), QImage::Format_RGB888);
//    glPushAttrib(GL_CLIENT_PIXEL_STORE_BIT);
//    glPixelStorei(GL_PACK_ALIGNMENT, 1);
//    glReadPixels(0, 0, width(), height(), GL_RGB, GL_UNSIGNED_BYTE, links.bits());
    //links.mirrored().save(QString("links%1.png").arg(counter));

    static int counter = 0;

    glFinish();

    PROFILE_RESULT("  GL wait")

    QImage links = pbuffer->toImage();
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

    //glPopAttrib();

    //links.save("fbo.png");
    if( _subscribe_links->isValid() )
    {
      //writeTexture(_subscribe_links, QString("links%1.png").arg(counter));

      QBitmap mask = QBitmap::fromImage( links.createMaskFromColor( qRgba(0,0,0, 0) ) );
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
    _image = links;

    PROFILE_RESULT("  updateMask")

    update();

    PROFILE_RESULT("  update")
	/*
	QPixmap screen = QPixmap::fromImage( grabFrameBuffer(true) );
	screen.save(QString("links%1.png").arg(counter));*/

    // TODO render flipped to not need mirror anymore
  }

  //----------------------------------------------------------------------------
  void GLWidget::startRender()
  {
    _render_thread->start();
  }

  //----------------------------------------------------------------------------
  void GLWidget::waitForData()
  {
    QMutexLocker lock(&_mutex_slot_links);
    _cond_render.wait(&_mutex_slot_links, 200);
  }

  //----------------------------------------------------------------------------
  void GLWidget::paintEvent(QPaintEvent *event)
  {
    QPainter painter(this);

    if( _subscribe_xray->_data->img )
    {
      painter.drawImage( _subscribe_xray->_data->region.toQRect(),
                         *_subscribe_xray->_data->img );
    }

    painter.drawImage(QPoint(0,0), _image);

    for( auto popup = _slot_popups->_data->popups.begin();
              popup != _slot_popups->_data->popups.end();
            ++popup )
    {
      if( popup->region.visible )
        painter.drawText
        (
          popup->region.region.pos.x,
          popup->region.region.pos.y - popup->region.region.size.y - 8,
          popup->region.region.size.x,
          popup->region.region.size.y,
          Qt::AlignCenter,
          QString::fromStdString(popup->text)
        );

      if( !popup->hover_region.visible )
        continue;

//      const LinksRouting::SlotType::Image* img = _slot_image->_data.get();
//      const float2& pos = popup->hover_region.region.pos;
//      if( img->pdata )
//      {
//        painter.drawImage(
//          QRect(pos.x + 8, pos.y - _window_offset.y() + 8, 256, 384),
//          QImage(img->pdata, img->width, img->height, QImage::Format_ARGB32),
//          QRect(0, 0, 128, 192)
//        );
//      }
    }
  }

  //----------------------------------------------------------------------------
  void GLWidget::resizeEvent(QResizeEvent * event)
  {
    int w = event->size().width(),
        h = event->size().height();

    LOG_ENTER_FUNC() << "(" << w << "|" << h << ")"
                     << static_cast<float>(w)/h << ":" << 1;

    _render_thread->resize(w, h);
  }

  //----------------------------------------------------------------------------
  void GLWidget::moveEvent(QMoveEvent *event)
  {
    // X11: Window decorators add decoration after creating the window.
    // Win7: may not allow a window on top of the task bar
    //So we need to get the new frame of the window before drawing...
    QPoint window_offset = mapToGlobal(QPoint());
    QPoint window_end = mapToGlobal(QPoint(width(), height()));

    if(    window_offset != _window_offset
        || window_end != _window_end )
    {
//      LOG_ENTER_FUNC() << "window frame="
//                       << window_offset
//                       <<" <-> "
//                       <<  window_end;

      _window_offset = window_offset;
      _window_end = window_end;

      // Moves the window to the top- and leftmost position. As long as the
      // offset changes either the user is moving the window or the window is
      // being created.
      move( QPoint(0,0) );
    }
  }
  
  //----------------------------------------------------------------------------
  void GLWidget::mouseReleaseEvent(QMouseEvent *event)
  {
    if( _do_drag )
      _do_drag = false;
    else
      _slot_mouse->_data->triggerClick(event->globalX(), event->globalY());
  }

  //----------------------------------------------------------------------------
  void GLWidget::mouseMoveEvent(QMouseEvent *event)
  {
    if( !event->buttons() )
      _slot_mouse->_data->triggerMove(event->globalX(), event->globalY());
    else if( event->buttons() & Qt::LeftButton )
    {
      float2 pos(event->globalX(), event->globalY());
      if( _do_drag )
        _slot_mouse->_data->triggerDrag(pos - _last_mouse_pos);
      else
        _do_drag = true;
      _last_mouse_pos = pos;
    }
  }
  
  //----------------------------------------------------------------------------
  void GLWidget::leaveEvent(QEvent *event)
  {
    _slot_mouse->_data->triggerLeave();

    bool change = false;
#if 0
    for( auto popup = _slot_popups->_data->popups.begin();
                  popup != _slot_popups->_data->popups.end();
                ++popup )
    {
      if( popup->visible )
      {
        popup->visible = false;
        change = true;
      }
    }
#endif
    if( change )
      _cond_render.wakeAll();
  }

  //----------------------------------------------------------------------------
  void GLWidget::wheelEvent(QWheelEvent *event)
  {
    _slot_mouse->_data->triggerScroll(
      event->delta(),
      float2(event->globalX(), event->globalY()),
      event->modifiers()
    );
  }

  //----------------------------------------------------------------------------
  void GLWidget::updateScreenShot( QPoint window_offset,
                                   QPoint window_end,
                                   QGLPixelBuffer* pbuffer )
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
    GLuint tid = pbuffer->bindTexture(_screenshot, GL_TEXTURE_2D);//, GL_RGBA, QGLContext::NoBindOption);

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
