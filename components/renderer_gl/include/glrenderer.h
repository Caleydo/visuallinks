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

      std::vector<Color>    _colors;
      Color                 _color_cur,
                            _color_covered_cur;

      Partitions *_partitions_src,
                 *_partitions_dest;

      /** Subscribe to the routed links */
      slot_t<LinkDescription::LinkList>::type _subscribe_links;
      slot_t<SlotType::TextPopup>::type _subscribe_popups;
      slot_t<SlotType::XRayPopup>::type _subscribe_xray;

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
      bool renderNodes( const LinkDescription::nodes_t& nodes,
                        float line_width = 3,
                        HyperEdgeQueue* hedges_open = NULL,
                        HyperEdgeSet* hedges_done = NULL,
                        bool render_all = false,
                        int pass = 0 );
      bool renderRect( const Rect& rect,
                       size_t margin = 2,
                       GLuint tex = 0,
                       const Color& fill = Color(1, 1, 1, 1),
                       const Color& border = Color(0.3, 0.3, 0.3, 0.8) );
      bool renderTileMap( const HierarchicTileMapPtr& tile_map,
                          const Rect& src_region,
                          const Rect& target_region,
                          size_t zoom = -1,
                          bool auto_center = false );

      float2 glVertex2f(float x, float y);
  };
}

#endif //LR_GLRENDERER
