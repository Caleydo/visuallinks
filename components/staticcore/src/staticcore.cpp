#include "staticcore.h"
#include "log.hpp"

namespace LinksRouting
{
  StaticCore::StaticCore():
    Configurable("StaticCore"),
    _runningComponents(0),
    _config(0),
    _user_config(0),
    _default_routing("CPURouting")
  {
#ifdef _DEBUG
    _requiredComponents = 0;
#else
    _requiredComponents = Component::Costanalysis
                        | Component::Routing
                        | Component::Renderer;
#endif

    registerArg("DefaultRouting", _default_routing);
  }

  StaticCore::~StaticCore()
  {
    shutdown();
  }

  //----------------------------------------------------------------------------
  SlotCollector StaticCore::getSlotCollector()
  {
    return SlotCollector(_slots);
  }

  //----------------------------------------------------------------------------
  SlotSubscriber StaticCore::getSlotSubscriber()
  {
    return SlotSubscriber(_slots);
  }

  //----------------------------------------------------------------------------
  bool StaticCore::startup(const std::string& startup)
  {
    _startupstr = startup;
    return true;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::attachComponent(Component* comp, unsigned int type)
  {
    LOG_INFO("Adding component (" << comp->name() << ")...");

    if( (type & Component::Config) && comp->supports(Component::Config) )
    {
      Config* config = dynamic_cast<Config*>(comp);
      assert(config);

      if( !_config )
      {
        _config = config;
        LOG_INFO("Attach config (" << config << ")");
      }
      else if( !_user_config )
      {
        _user_config = config;
        LOG_INFO("Attach user config (" << config << ")");
      }

//      if( !_config->initFrom(_startupstr) )
//      {
//        LOG_ERROR("could not load config file \"" << _startupstr << "\"");
//        _config = nullptr;
//      }
    }

    unsigned int supportedTypes = getTypes(comp, type);
    unsigned int isRunningAs = supportedTypes & ~_runningComponents;

    if( !comp->startup(this, isRunningAs) )
      isRunningAs = 0;

    //update running components
    _runningComponents |= isRunningAs;
    _components.push_back(ComponentInfo( comp, supportedTypes, isRunningAs));

    return isRunningAs != 0;
  }

  Component* StaticCore::getComponent(Component::Type type)
  {
    Component* found = 0;
    for(auto c = _components.begin(); c != _components.end(); ++c)
    {
      if(found == 0 && (c->canbe & type) != 0)
        found = *c;
      if((c->is & type) != 0)
      {
        found = *c;
        break;
      }
    }
    return found;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::init()
  {
//    if( (_runningComponents & _requiredComponents) != _requiredComponents)
//    {
//      LOG_ERROR("minimum of runable components not provided!");
//      shutdown();
//      return false;
//    }

    if( _config )
      initConfig(_config);
    if( _user_config )
      initConfig(_user_config);

    if( !_config && !_user_config )
      std::cout << "LinksRouting StaticCore Warning: no config provided." << std::endl;

    _slot_select_routing =
      getSlotCollector().create<SlotType::ComponentSelection>("/routing");
    _slot_select_routing->_data->linkDefault(&_default_routing);

    _slot_user_config = getSlotCollector().create<Config*>("/user-config");
    *_slot_user_config->_data = _user_config;

    for( auto c = _components.begin(); c != _components.end(); ++c )
    {
      c->comp->init();
      if( c->comp->supports(Component::Routing) )
      {
        _slot_select_routing->_data->available[ c->comp->name() ] = true;
        c->is = 0;
      }
    }

    std::cout << "Available routing components: " << std::endl;
    for( auto comp = _slot_select_routing->_data->available.begin();
         comp != _slot_select_routing->_data->available.end();
         ++comp )
      std::cout << " - " << comp->first << std::endl;

    SlotCollector slot_collector(_slots);
    for( auto c = _components.begin(); c != _components.end(); ++c )
      c->comp->publishSlots(slot_collector);

    SlotSubscriber slot_subscriber(_slots);
    for( auto c = _components.begin(); c != _components.end(); ++c )
      c->comp->subscribeSlots(slot_subscriber);

    return true;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::initGL()
  {
    for( auto c = _components.begin(); c != _components.end(); ++c )
    {
      if( !c->comp->initGL() )
      {
        LOG_INFO("Shutting down unusable component: " << c->comp->name());
        c->comp->shutdown();
        c->is = 0;

        _slot_select_routing->_data->available[ c->comp->name() ] = false;
        if( _slot_select_routing->_data->active == c->comp->name() )
          _slot_select_routing->_data->active.clear();
      }
    }

    return true;
  }

  void StaticCore::shutdown()
  {
    for( auto c = _components.begin(); c != _components.end(); ++c )
      if(c->is != 0)
      {
        c->comp->shutdown();
        c->is = 0;
      }
  }

  //----------------------------------------------------------------------------
  uint32_t StaticCore::process(unsigned int type)
  {
    // Check if we need to select a new routing algorithm
    std::string request = _slot_select_routing->_data->request,
                active = _slot_select_routing->_data->active;
    if( !request.empty() || active.empty() )
    {
      if( request.empty() )
        request = _slot_select_routing->_data->getDefault();

      for( auto c = _components.rbegin(); c != _components.rend(); ++c )
        if(    c->comp->supports(Component::Routing)
            && _slot_select_routing->_data->available[ c->comp->name() ] )
        {
          if( !request.empty() && c->comp->name() != request )
          {
            c->is = 0;
            continue;
          }

          _slot_select_routing->_data->active = c->comp->name();
          c->is = Component::Routing;

          if( request.empty() )
            break;
        }

      std::cout << "Selected routing algorithm: "
                << _slot_select_routing->_data->active
                << std::endl;
      _slot_select_routing->_data->request.clear();
    }

    uint32_t flags = 0;
    for( auto c = _components.begin(); c != _components.end(); ++c )
    {
      if( c->is && c->comp->supports(type) )
      {
//        clock::time_point lap = clock::now();
//        std::cout << "+->" << c->comp->name() << std::endl;
        flags |= c->comp->process(type);
//        std::cout << c->comp->name() << " -> " << (clock::now() - lap) << std::endl;
      }
//      else
//        std::cout << "!->" << c->comp->name() << std::endl;
    }

//    std::cout << "staticcore: "
//              << (flags & Component::LINKS_DIRTY ? "links " : "")
//              << (flags & Component::RENDER_DIRTY ? "render " : "")
//              << (flags & Component::MASK_DIRTY ? "mask " : "")
//              << std::endl;

    return flags;
  }

  //----------------------------------------------------------------------------
  Config* StaticCore::getConfig()
  {
    return _config;
  }

  unsigned int StaticCore::getTypes(Component* component, unsigned int mask)
  {
    unsigned int type = 0;
    for(unsigned int i = 1; i < Component::Any; i <<= 1)
      if((i & mask) && component->supports(static_cast<Component::Type>(i)))
        type |= i;
    return type;
  }

  //----------------------------------------------------------------------------
  void StaticCore::initConfig(Config* config)
  {
    for( auto c = _components.begin(); c != _components.end(); ++c )
      config->attach(*c, c->is);
    config->attach(this, 0);
    config->process(Component::Config);
  }

}
