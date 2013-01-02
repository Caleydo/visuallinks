#ifndef LR_RENDERER
#define LR_RENDERER

#include <component.h>
#include <linkdescription.h>

namespace LinksRouting
{
  class Renderer:
    public virtual Component
  {
    protected:
      Renderer():
        Configurable("Renderer")
      {}
  };
} // namespace LinksRouting

#endif //LR_RENDERER
