#ifndef LR_CORE
#define LR_CORE
#include <component.h>

namespace LinksRouting
{
  class Config;
  class Core
  {
  public:
    virtual bool startup(const std::string& startup) = 0;
    virtual bool attachComponent(Component* comp, unsigned int type = Component::Any) = 0;
    virtual Component* getComponent(Component::Type type) = 0;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void process() = 0;

    virtual Config* getConfig() = 0;

    virtual ~Core() {}
  };
};

#endif //LR_CORE