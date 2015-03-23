/*!
 * @file application.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 14.10.2011
 */

#include "application.hpp"
#include "qt_helper.hxx"
#include "PreviewWindow.hpp"
#include "Window.hpp"

#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QScreen>

#include <iostream>

namespace qtfullscreensystem
{

  class QtPreview:
    public LinksRouting::SlotType::Preview
  {
    public:
      virtual
      LinksRouting::PreviewWindow*
      getWindow( LinksRouting::SlotType::TextPopup::Popup *popup,
                 uint8_t dev_id = 0 )
      {
        Application* app = dynamic_cast<Application*>(QApplication::instance());
        if( !app )
        {
          qWarning() << "no app";
          return nullptr;
        }

        LR::SlotSubscriber slot_subscriber = app->getSlotSubscriber();

        QtPreviewWindow* w = new QtPreviewWindow(popup);
        w->subscribeSlots(slot_subscriber);

        return w;
      }

      virtual
      LinksRouting::PreviewWindow*
      getWindow( LinksRouting::SlotType::XRayPopup::HoverRect *see_through,
                 uint8_t dev_id = 0 )
      {
        Application* app = dynamic_cast<Application*>(QApplication::instance());
        if( !app )
        {
          qWarning() << "no app";
          return nullptr;
        }

        LR::SlotSubscriber slot_subscriber = app->getSlotSubscriber();

        QtPreviewWindow* w = new QtPreviewWindow(see_through);
        w->subscribeSlots(slot_subscriber);

        return w;
      }
  };

  //----------------------------------------------------------------------------
  Application::Application(int& argc, char *argv[]):
    Configurable("Application"),
    QApplication(argc, argv),
    _server(&_mutex_slot_links, &_cond_render)
  {
//    _cur_fbo(0),
//    _do_drag(false),
//    _flags(0),

    setOrganizationDomain("icg.tugraz.at");
    setOrganizationName("icg.tugraz.at");
    setApplicationName("VisLinks");

    const QDir config_dir =
      QStandardPaths::writableLocation(QStandardPaths::DataLocation);
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
    // Setup component system
    //--------------------------------

    if( argc != 2 )
      qFatal("Usage: %s <config>", argv[0]);

    assert(argc == 2 && argv && argv[1]);

//    publishSlots(_core.getSlotCollector());

    if( !_config.initFrom(argv[1]) )
      qFatal("Failed to read config");
    _user_config.initFrom( to_string(user_config) );

    _core.startup();
    _core.attachComponent(&_config);
    _core.attachComponent(&_user_config);
    _core.attachComponent(&_server);
    _core.attachComponent(&_routing_cpu);
    _core.attachComponent(&_routing_cpu_dijkstra);
    _core.attachComponent(&_routing_dummy);
#ifdef USE_GPU_ROUTING
    _core.attachComponent(&_cost_analysis);
    _core.attachComponent(&_routing_gpu);
#endif
    _core.attachComponent(&_renderer);

    _core.attachComponent(this);
//    registerArg("DebugDesktopImage", _debug_desktop_image);
//    registerArg("DumpScreenshot", _dump_screenshot = 0);

    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
    fmt.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
    fmt.setAlphaBufferSize(8);

    _gl_ctx.setFormat(fmt);
    if( !_gl_ctx.create() )
      qFatal("Failed to create OpenGL context.");
    if( !_gl_ctx.format().hasAlpha() )
      qFatal("Failed to enable alpha blending.");

    _offscreen_surface.setFormat(fmt);
    _offscreen_surface.setScreen( QGuiApplication::primaryScreen() );
    _offscreen_surface.create();

    for(QScreen* s: QGuiApplication::screens())
    {
      auto w = std::make_shared<RenderWindow>(s->availableGeometry());
      w->setImage(&_fbo_image);
      w->show();
      _windows.push_back(w);

      auto mw = std::make_shared<RenderWindow>(s->availableGeometry());
      mw->show();
      _mask_windows.push_back(mw);
    }

    _core.init();

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(200);
  }

  //----------------------------------------------------------------------------
  Application::~Application()
  {

  }

  //----------------------------------------------------------------------------
  void Application::publishSlots(LR::SlotCollector& slot_collector)
  {
    _slot_desktop =
      slot_collector.create<LR::SlotType::Image>("/desktop");
    _slot_desktop->_data->type = LR::SlotType::Image::OpenGLTexture;

    _slot_desktop_rect =
      slot_collector.create<Rect>("/desktop/rect");
    *_slot_desktop_rect->_data =
      QGuiApplication::primaryScreen()->availableVirtualGeometry();
    _slot_desktop_rect->setValid(true);

    _slot_mouse =
      slot_collector.create<LR::SlotType::MouseEvent>("/mouse");
    _slot_popups =
      slot_collector.create<LR::SlotType::TextPopup>("/popups");
    _slot_previews =
      slot_collector.create<LR::SlotType::Preview, QtPreview>("/previews");
  }

  //----------------------------------------------------------------------------
  void Application::subscribeSlots(LR::SlotSubscriber& slot_subscriber)
  {
    _subscribe_links =
      slot_subscriber.getSlot<LR::SlotType::Image>("/rendered-links");
    _subscribe_xray_fbo =
      slot_subscriber.getSlot<LR::SlotType::Image>("/rendered-xray");
#ifdef USE_GPU_ROUTING
    _subscribe_costmap =
      slot_subscriber.getSlot<LR::SlotType::Image>("/costmap");
#endif
    _subscribe_routed_links =
      slot_subscriber.getSlot<LR::LinkDescription::LinkList>("/links");
    _subscribe_outlines =
      slot_subscriber.getSlot<LR::SlotType::CoveredOutline>("/covered-outlines");

    for(auto& w: _windows)
      w->subscribeSlots(slot_subscriber);
    for(auto& w: _mask_windows)
      w->subscribeSlots(slot_subscriber);
  }

  //----------------------------------------------------------------------------
  LR::SlotCollector Application::getSlotCollector()
  {
    return _core.getSlotCollector();
  }

  //----------------------------------------------------------------------------
  LR::SlotSubscriber Application::getSlotSubscriber()
  {
    return _core.getSlotSubscriber();
  }

  //----------------------------------------------------------------------------
  void Application::update()
  {
    int pass = 1;
    uint32_t _flags = LINKS_DIRTY | RENDER_DIRTY;

    if( !_gl_ctx.makeCurrent(&_offscreen_surface) )
      qFatal("Could not activate OpenGL context.");

    if( !_fbo )
    {
      QSize size = QGuiApplication::primaryScreen()->availableVirtualSize();
      qDebug() << "initFBO" << size;
      _fbo.reset(new QOpenGLFramebufferObject(size));

      _shader_blend = Shader::loadFromFiles("simple.vert", "blend.frag");
      if( !_shader_blend )
        qFatal("Failed to load blend shader.");

      glClearColor(0, 0, 0, 0);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glViewport(0,0, _fbo->width(), _fbo->height());

      _core.initGL();
    }

    Rect const& desktop = *_slot_desktop_rect->_data;
    glMatrixMode(GL_PROJECTION);
    glOrtho(desktop.l(), desktop.r(), desktop.t(), desktop.b(), -1.0, 1.0);

    uint32_t old_flags = _flags;
    {
      QMutexLocker lock_links(&_mutex_slot_links);
      uint32_t types = pass == 0
                     ? (Component::Renderer | 64)
                     :   Component::Config
                       | Component::DataServer
                       | ((_flags & LINKS_DIRTY) ? Component::Routing : 0)
                       | ((_flags & RENDER_DIRTY) ? Component::Renderer : 0);

//      std::cout << "types: " << (types & Component::Routing ? "routing " : "")
//                             << (types & Component::Renderer ? "render " : "")
//                             << std::endl;

      _flags = _core.process(types);
    }

    static int counter = 0;
    ++counter;

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

    glFinish();
    //writeTexture(_subscribe_links, QString("links%1.png").arg(counter));

    if( !_fbo->bind() )
      qFatal("Failed to bind FBO.");

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

#if 1
    GLuint tex_ids[] = {
      _subscribe_xray_fbo->_data->id,
      _subscribe_links->_data->id,
#ifndef USE_DESKTOP_BLEND
      _slot_desktop->_data->id
#endif
    };
    size_t num_textures = sizeof(tex_ids)/sizeof(tex_ids[0]);

    for(size_t i = 0; i < num_textures; ++i)
    {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, tex_ids[i]);
    }

    _shader_blend->bind();
    _shader_blend->setUniformValue("xray", 0);
    _shader_blend->setUniformValue("links", 1);
#ifndef USE_DESKTOP_BLEND
    _shader_blend->setUniformValue("desktop", 2);
#endif
#if 0
    _shader_blend->setUniformValue("mouse_pos", pos);
#endif

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

    _shader_blend->release();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

#endif
    if( !_fbo->release() )
      qFatal("Failed to release FBO.");

    glFinish();

    _fbo_image = _fbo->toImage();
    //_fbo_image.save(QString("fbo%1.png").arg(counter));

    int timeout = 5;
    for(size_t i = 0; i < _windows.size(); ++i)
    {
      //w->update();
      QTimer::singleShot(timeout, _windows[i].get(), SLOT(update()));
      QTimer::singleShot(timeout, _mask_windows[i].get(), SLOT(update()));

      // Waiting a bit between updates seems to reduce artifacts...
      timeout += 30;
    }

    _gl_ctx.doneCurrent();
  }

} // namespace qtfullscreensystem
