#ifndef LR_CONFIG
#define LR_CONFIG

#include <component.h>
#include <vector>

namespace LinksRouting
{
  class Config : public virtual Component
  {
  public:
    virtual bool initFrom(const std::string& config) = 0;
    virtual void attach(Component* component, unsigned int type) = 0;
  };
};


#endif
