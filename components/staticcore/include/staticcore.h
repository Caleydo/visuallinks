#ifndef LR_STATICCORE
#define LR_STATICCORE

#include <core.h>
#include <config.h>
#include <costanalysis.h>
#include <proxy.h>
#include <renderer.h>
#include <routing.h>
#include <transparencyanalysis.h>

#include "slots.hpp"

#include <list>
#include <stdexcept>

namespace LinksRouting
{
  typedef std::vector<Component*> components_t;

  class StaticCore: public Core
  {
      struct ComponentInfo
      {
          Component* comp;
          unsigned int canbe;
          unsigned int is;
          ComponentInfo(Component* c, unsigned int can) :
                comp(c),
                canbe(can),
                is(0)
          {
          }
      };
      typedef std::list<ComponentInfo> ComponentList;
      ComponentList components;
      struct RunningComponent
      {
          ComponentInfo* compinfo;
          unsigned int type;
          RunningComponent(ComponentInfo* info, unsigned int t) :
                compinfo(info),
                type(t)
          {
          }
          bool operator <(const RunningComponent& other)
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

      SlotCollector getSlotCollector();
      template<typename DataType>
      typename slot_t<DataType>::type getSlot(const std::string& name)// const
      {
        auto slot = _slots.find(name);

        if( slot == _slots.end() )
          throw std::runtime_error("No such slot: " + name);

        return std::dynamic_pointer_cast<typename slot_t<DataType>::raw_type>(slot->second.lock());
      }

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

      /** Slots used for communication between components */
      slots_t _slots;

  };
}
;

#endif
