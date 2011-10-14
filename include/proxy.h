#ifndef LR_PROXY
#define LR_PROXY
#include <component.h>

namespace LinksRouting
{
  class Proxy : public virtual Component
  {
  public:
    virtual bool serve(Component* component) = 0;

    virtual void processProxy() = 0;
  };
};


#endif //LR_PROXY