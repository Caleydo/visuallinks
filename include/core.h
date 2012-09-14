#ifndef LR_CORE
#define LR_CORE

#include <common/componentarguments.h>

namespace LinksRouting
{
  class Config;
  class Component;
  class Core:
    public ComponentArguments
  {
    public:

      virtual bool startup(const std::string& startup = std::string()) = 0;
      virtual bool attachComponent(Component* comp, unsigned int type =
                                     Component::Any) = 0;
      virtual Component* getComponent(Component::Type type) = 0;
      virtual bool init() = 0;
      virtual void shutdown() = 0;
      virtual void process(unsigned int type) = 0;

      virtual Config* getConfig() = 0;

    protected:

      Core():
        Configurable("Core")
      {}
  };
}

#endif //LR_CORE
