#ifndef LR_STATICCORE
#define LR_STATICCORE

#include <core.h>
#include <config.h>
#include <costanalysis.h>
#include <proxy.h>
#include <renderer.h>
#include <routing.h>
#include <transparencyanalysis.h>

#include <list>

namespace LinksRouting
{
  class StaticCore : public Core
  {
    struct ComponentInfo
    {
      Component* comp;
      unsigned int canbe;
      unsigned int is;
      ComponentInfo(Component* c, unsigned int can) : comp(c), canbe(can), is(0) { }
    };
    typedef std::list<ComponentInfo> ComponentList;
    ComponentList components;
    struct RunningComponent
    {
      ComponentInfo* compinfo;
      unsigned int type;
      RunningComponent(ComponentInfo* info, unsigned int t) : compinfo(info), type(t) { }
      bool operator < (const RunningComponent& other)
      {
        return type < other.type;
      }
    };
    typedef std::list<RunningComponent> RunningComponentList;
    RunningComponentList running_components;
    Config* config;

    unsigned int requiredComponents;
    std::string startupstr;

    void connectComponents();
    Component* findRunningComponent(Component::Type type);
    static unsigned int getTypes(Component* component, unsigned int mask);
  public:
    StaticCore();
    ~StaticCore();

    virtual bool startup(const std::string& startup);
    virtual bool attachComponent(Component* comp, unsigned int type = Component::Any);
    virtual Component* getComponent(Component::Type type);
    virtual bool init();
    virtual void shutdown();
    virtual void process();

    virtual Config* getConfig();
  };
};

#endif