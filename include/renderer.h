#ifndef LR_RENDERER
#define LR_RENDERER
#include <component.h>
#include <linkdescription.h>

namespace LinksRouting
{
  class Renderer : public virtual Component
  {
  public:
//    virtual bool setTransparencyInput(const Component::MapData& inputmap) = 0;
    virtual bool addLinkHierarchy(LinkDescription::Node* node) = 0;
    virtual bool addLinkHierarchy(LinkDescription::HyperEdge* hyperedge) = 0;
    virtual bool removeLinkHierarchy(LinkDescription::Node* node) = 0;
    virtual bool removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge) = 0;
  };
};


#endif //LR_RENDERER
