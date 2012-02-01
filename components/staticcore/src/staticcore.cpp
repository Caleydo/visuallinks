#include "staticcore.h"

#include <iostream>

namespace LinksRouting
{
  StaticCore::StaticCore() : _config(0), _runningComponents(0)
  {
#ifdef _DEBUG
    _requiredComponents = 0;
#else
    _requiredComponents = Component::Costanalysis | Component::Routing | Component::Renderer;
#endif
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

  bool StaticCore::startup(const std::string& startup)
  {
    _startupstr = startup;
    return true;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::attachComponent(Component* comp, unsigned int type)
  {
    std::cout << "[StaticCore] Adding component (" << comp->name() << ")..."
              << std::endl;

    if(!_config && (type&Component::Config) && comp->supports(Component::Config))
      _config = dynamic_cast<Config*>(comp);

    unsigned int supportedTypes = getTypes(comp, type);
    unsigned int isRunningAs = supportedTypes & ~_runningComponents;

    if(!comp->startup(this, isRunningAs))
          isRunningAs = 0;

    //update running components (ignore proxies)
    _runningComponents |= (isRunningAs & ~Component::Proxy);
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
    if( (_runningComponents & _requiredComponents) != _requiredComponents)
    {
      std::cerr << "LinksRouting StaticCore Error: minimum of runable components not provided!" << std::endl;
      shutdown();
      return false;
    }

    if(_config)
    {
      if(!_config->initFrom(_startupstr))
        std::cout << "LinksRouting StaticCore Warning: could not load config file \"" << _startupstr << "\"" << std::endl;
      for( auto c = _components.begin(); c != _components.end(); ++c )
          _config->attach(*c, c->is);
      _config->process(Component::Config);
    }
    else
      std::cout << "LinksRouting StaticCore Warning: no config provided." << std::endl;

    for( auto c = _components.begin(); c != _components.end(); ++c )
      if(c->comp != static_cast<Component*>(_config))
        c->comp->init();

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
      c->comp->initGL();

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
  void StaticCore::process()
  {
    for( auto c = _components.begin(); c != _components.end(); ++c )
      c->comp->process();
  }

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

}
