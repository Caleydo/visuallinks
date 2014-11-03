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

      uint32_t process(unsigned int type) override;

      virtual bool setString(const std::string& name, const std::string& val);

    protected:

      std::vector<Color>    _colors;
      Color                 _color_cur,
                            _color_covered_cur;
      int                   _num_blur;

      unsigned int _margin_left;

      /** Subscribe to the routed links */
      slot_t<LinkDescription::LinkList>::type _subscribe_links;
      slot_t<SlotType::TextPopup>::type _subscribe_popups;
      slot_t<SlotType::XRayPopup>::type _subscribe_xray;
      slot_t<SlotType::CoveredOutline>::type _subscribe_outlines;

      /** Publish the links rendered to fbo */
      slot_t<SlotType::Image>::type _slot_links,
                                    _slot_xray;

      /** Frame buffer object where links get rendered to */
      gl::FBO   _links_fbo,
                _xray_fbo;

      cwc::glShaderManager  _shader_manager;
      cwc::glShader*        _blur_x_shader;
      cwc::glShader*        _blur_y_shader;

      typedef std::queue<const LinkDescription::HyperEdge*> HyperEdgeQueue;
      typedef std::set<const LinkDescription::HyperEdge*> HyperEdgeSet;

      Color getCurrentColor() const;

      void blur(gl::FBO& fbo);

      bool renderLinks( const LinkDescription::LinkList& links,
                        int pass = 0 );
  };
}

#endif //LR_GLRENDERER
