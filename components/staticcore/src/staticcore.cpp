#include "staticcore.h"

#include <iostream>

namespace LinksRouting
{
  StaticCore::StaticCore() : config(0)
  {
    requiredComponents = Component::Costanalysis;// | Component::Routing | Component::Renderer;
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

  bool StaticCore::startup(const std::string& startup)
  {
    startupstr = startup;
    return true;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::attachComponent(Component* comp, unsigned int type)
  {
    std::cout << "[StaticCore] Adding component (" << comp->name() << ")..."
              << std::endl;

    _components.push_back(comp);
    //add component to the list
//    components.push_back(ComponentInfo(comp, getTypes(comp, type)));
    return true;
  }

  Component* StaticCore::getComponent(Component::Type type)
  {
    Component* found = 0;
    ComponentList::iterator it = components.begin();
    for(; it != components.end(); ++it)
    {
      if(found == 0 && (it->canbe & type) != 0)
        found = it->comp;
      if((it->is & type) != 0)
      {
        found = it->comp;
        break;
      }
    }
    return found;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::init()
  {
    SlotCollector slot_collector(_slots);
    for( auto c = _components.begin(); c != _components.end(); ++c )
      (*c)->publishSlots(slot_collector);

    for( auto c = _components.begin(); c != _components.end(); ++c )
      (*c)->subscribeSlots(_slots);

#if 0
    //try to put together a running system
    unsigned int runningTypes = 0;

    ComponentList::iterator it = components.begin();
    for(; it != components.end(); ++it)
    {
      //ignore proxies
      if((it->canbe & ~Component::Proxy) == 0)
        continue;
      unsigned int makeit =  ~runningTypes & it->canbe;
      if(makeit != 0)
      {
        if(!it->comp->startup(this, makeit))
          continue;
        it->is = makeit;
        runningTypes |= makeit;
        unsigned int workas = makeit;
        for(unsigned int tester = 1; workas != 0; tester<<=1)
        {
          if( (workas & tester) != 0)
          {
            running_components.push_back(RunningComponent(&(*it), tester));
            workas &= ~tester;
          }
        }
        if(it->is & Component::Config)
          config = dynamic_cast<Config*>(it->comp);
      }
    }

    if( (runningTypes & requiredComponents) != requiredComponents)
    {
      std::cerr << "LinksRouting StaticCore Error: minimum of runable components not provided!" << std::endl;
      shutdown();
      return false;
    }
    //attach proxies
    it = components.begin();
    for(; it != components.end(); ++it)
    {
      if((it->canbe & Component::Proxy) != 0 && it->is == 0)
      {
        Proxy* proxy = dynamic_cast<Proxy*>(it->comp);
        proxy->startup(this, Component::Proxy);
        bool canserve = false;
        for(RunningComponentList::iterator rit = running_components.begin(); rit != running_components.end(); ++rit)
        {
          canserve |= proxy->serve(rit->compinfo->comp);
        }
        if(canserve)
          running_components.push_back(RunningComponent(&(*it),Component::Proxy));
        else
          proxy->shutdown();
      }
    }

    //sort
    running_components.sort();
    //connect
    connectComponents();

    //attach to config
    if(config)
    {
      if(!startupstr.empty())
        config->initFrom(startupstr);
      it = components.begin();
      for(; it != components.end(); ++it)
        if(it->is != 0)
          config->attach(it->comp, it->is);
      config->process(Component::Config);
    }
    else
      std::cout << "LinksRouting StaticCore Warning: no config provided." << std::endl;

    //init
    //RunningComponentList::iterator rit = running_components.begin();
    it = components.begin();
    for(; it != components.end(); ++it)
      if(it->is != 0)
        it->comp->init();
#endif
    return true;
  }

  //----------------------------------------------------------------------------
  bool StaticCore::initGL()
  {
    for( auto c = _components.begin(); c != _components.end(); ++c )
      (*c)->initGL();

    return true;
  }

  void StaticCore::shutdown()
  {
    ComponentList::iterator it = components.begin();
    for(; it != components.end(); ++it)
      if(it->is != 0)
      {
        it->comp->shutdown();
        it->is = 0;
      }
  }
  void StaticCore::process()
  {
    for( auto c = _components.begin(); c != _components.end(); ++c )
      (*c)->process();

//    RunningComponentList::iterator it = running_components.begin();
//    for(;it != running_components.end(); ++it)
//      it->compinfo->comp->process(static_cast<Component::Type>(it->type));
  }

  Config* StaticCore::getConfig()
  {
    return config;
  }

  void StaticCore::connectComponents()
  {
    RunningComponentList::iterator it = running_components.begin();
    for(;it != running_components.end(); ++it)
    {
      switch(it->type)
      {
      case Component::TransparencyAnalysis:
        {
          TransparencyAnalysis* ta = dynamic_cast<TransparencyAnalysis*>(it->compinfo->comp);
          CostAnalysis* ca = dynamic_cast<CostAnalysis*>(findRunningComponent(Component::Costanalysis));
          Renderer* r = dynamic_cast<Renderer*>(findRunningComponent(Component::Renderer));
          ta->connect(ca, r);
        }
        break;
      case Component::Costanalysis:
        {
          CostAnalysis* ca = dynamic_cast<CostAnalysis*>(it->compinfo->comp);
          Routing* r = dynamic_cast<Routing*>(findRunningComponent(Component::Routing));
          ca->connect(r);
        }
        break;
      case Component::Routing:
        {
          Routing* routing = dynamic_cast<Routing*>(it->compinfo->comp);
          CostAnalysis* ca = dynamic_cast<CostAnalysis*>(findRunningComponent(Component::Costanalysis));
          Renderer* r = dynamic_cast<Renderer*>(findRunningComponent(Component::Renderer));
          routing->connect(ca, r);
        }
        break;
      default:
        //nothing to do
        break;
      }
    }

  }
  Component* StaticCore::findRunningComponent(Component::Type type)
  {
    RunningComponentList::iterator it = running_components.begin();
    for(;it != running_components.end(); ++it)
    {
      if(it->type == type)
        return it->compinfo->comp;
    }
    return 0;
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
