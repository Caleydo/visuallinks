#ifndef LR_GLRENDERER
#define LR_GLRENDERER

#include <renderer.h>
#include <common/componentarguments.h>

#include "fbo.h"
#include "slots.hpp"
#include "slotdata/image.hpp"

namespace LinksRouting
{
  class GlRenderer: public Renderer, public ComponentArguments
  {
    protected:
      std::string myname;
    public:

      GlRenderer();
      virtual ~GlRenderer();

      virtual void publishSlots(SlotCollector& slots);

      bool startup(Core* core, unsigned int type);
      void init();
      void initGL();
      void shutdown();
      bool supports(Type type) const
      {
        return type == Component::Renderer;
      }
      const std::string& name() const
      {
        return myname;
      }

      void process(Type type);

//      bool setTransparencyInput(const Component::MapData& inputmap);
      bool addLinkHierarchy(LinkDescription::Node* node);
      bool addLinkHierarchy(LinkDescription::HyperEdge* hyperedge);
      bool removeLinkHierarchy(LinkDescription::Node* node);
      bool removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge);

    protected:

      /** Publish the links rendered to fbo */
      slot_t<SlotType::Image>::type _slot_links;

      /** Frame buffer object where links get rendered to */
      gl::FBO   _links_fbo;
  };
}

#endif //LR_GLRENDERER
