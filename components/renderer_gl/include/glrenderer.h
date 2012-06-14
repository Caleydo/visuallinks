#ifndef LR_GLRENDERER
#define LR_GLRENDERER

#include <renderer.h>
#include <common/componentarguments.h>

#include "fbo.h"
#include "glsl/glsl.h"
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

      void publishSlots(SlotCollector& slots);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

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

      virtual bool setString(const std::string& name, const std::string& val);

    protected:

      bool  _enabled;

      struct Color
      {
        float r, g, b;

        Color(){};
        Color(int r, int g, int b):
          r(r/256.f),
          g(g/256.f),
          b(b/256.f)
        {}
        operator const float*()
        {
          return &r;
        }
        Color operator*(float rhs) const
        {
          Color ret;
          ret.r = rhs * r;
          ret.g = rhs * g;
          ret.b = rhs * b;
          return ret;
        }
      };
      std::vector<Color> _colors;

      /** Subscribe to the routed links */
      slot_t<LinkDescription::LinkList>::type _subscribe_links;

      /** Publish the links rendered to fbo */
      slot_t<SlotType::Image>::type _slot_links;

      /** Frame buffer object where links get rendered to */
      gl::FBO   _links_fbo;

      cwc::glShaderManager  _shader_manager;
      cwc::glShader*    _blur_x_shader;
      cwc::glShader*    _blur_y_shader;

      void renderLinks(const LinkDescription::LinkList& links);
  };
}

#endif //LR_GLRENDERER
