#ifndef LR_GLRENDERER
#define LR_GLRENDERER

#include <string>

#include <renderer.h>
#include <common/componentarguments.h>

namespace LinksRouting
{
  class GlRenderer: public Renderer, public ComponentArguments
  {
    protected:
      std::string myname;
      //DUMMYTESt
      double TestDouble;
      std::string TestString;
    public:

      GlRenderer();
      virtual ~GlRenderer();

      bool startup(Core* core, unsigned int type);
      void init();
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

      bool setTransparencyInput(const Component::MapData& inputmap);
      bool addLinkHierarchy(LinkDescription::Node* node);
      bool addLinkHierarchy(LinkDescription::HyperEdge* hyperedge);
      bool removeLinkHierarchy(LinkDescription::Node* node);
      bool removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge);

  };
}
;
#endif //LR_GLRENDERER
