#ifndef LR_STATICCORE
#define LR_STATICCORE

#include <core.h>
#include <config.h>
#include <costanalysis.h>
#include <renderer.h>
#include <routing.h>
#include <transparencyanalysis.h>

#include "slots.hpp"

#include <list>
#include <stdexcept>

namespace LinksRouting
{
  class StaticCore: public Core
  {
      struct ComponentInfo
      {
          Component* comp;
          unsigned int canbe;
          unsigned int is;
          ComponentInfo(Component* c, unsigned int can, unsigned int now = 0) :
                comp(c),
                canbe(can),
                is(now)
          {
          }
          operator Component*() { return comp; }
      };

      typedef std::vector<ComponentInfo> components_t;

      static unsigned int getTypes(Component* component, unsigned int mask);

    public:

      StaticCore();
      ~StaticCore();

      SlotCollector getSlotCollector();
      SlotSubscriber getSlotSubscriber();

      virtual bool startup(const std::string& startup);
      virtual bool attachComponent(Component* comp, unsigned int type =
                                     Component::Any);
      virtual Component* getComponent(Component::Type type);
      virtual bool init();
      virtual bool initGL();
      virtual void shutdown();
      virtual void process();

      virtual Config* getConfig();

    private:

      /** The attached components */
      components_t _components;

      /** Types of components running */
      unsigned int _runningComponents;

      /** Slots used for communication between components */
      slots_t _slots;

      /** Config to be used */
      Config* _config;

      /** Requrired components */
      unsigned int _requiredComponents;

      /** Command line arguments */
      std::string _startupstr;

  };
}
;

#endif
