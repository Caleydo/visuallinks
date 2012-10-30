#ifndef LR_GLRENDERER
#define LR_GLRENDERER

#include <renderer.h>
#include <common/componentarguments.h>

#include "fbo.h"
#include "glsl/glsl.h"
#include "slots.hpp"
#include "slotdata/image.hpp"
#include "slotdata/text_popup.hpp"

#include <queue>
#include <set>

namespace LinksRouting
{
  class GlRenderer:
    public Renderer,
    public ComponentArguments
  {
    public:

      GlRenderer();
      virtual ~GlRenderer();

      void publishSlots(SlotCollector& slots);
      void subscribeSlots(SlotSubscriber& slot_subscriber);

      bool startup(Core* core, unsigned int type);
      void init();
      bool initGL();
      void shutdown();
      bool supports(unsigned int type) const
      {
        return (type & Component::Renderer);
      }

      void process(unsigned int type);

      virtual bool setString(const std::string& name, const std::string& val);

    protected:

      std::vector<Color> _colors;

      /** Subscribe to the routed links */
      slot_t<LinkDescription::LinkList>::type _subscribe_links;
      slot_t<LinksRouting::SlotType::TextPopup>::type _subscribe_popups;

      /** Publish the links rendered to fbo */
      slot_t<SlotType::Image>::type _slot_links;

      /** Overlay/preview image */
      slot_t<SlotType::Image>::type _slot_image;

      /** Frame buffer object where links get rendered to */
      gl::FBO   _links_fbo;

      cwc::glShaderManager  _shader_manager;
      cwc::glShader*        _blur_x_shader;
      cwc::glShader*        _blur_y_shader;

      GLuint    _tex_img_preview;

      typedef std::queue<const LinkDescription::HyperEdge*> HyperEdgeQueue;
      typedef std::set<const LinkDescription::HyperEdge*> HyperEdgeSet;

      void blur(gl::FBO& fbo);

      bool renderLinks(const LinkDescription::LinkList& links);
      bool renderNodes( const LinkDescription::nodes_t& nodes,
                        float line_width = 3,
                        HyperEdgeQueue* hedges_open = NULL,
                        HyperEdgeSet* hedges_done = NULL,
                        bool render_all = false );
      bool renderRect(const Rect& rect, size_t border = 2, GLuint tex = 0);
  };
}

#endif //LR_GLRENDERER
