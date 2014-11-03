/*
 * Request tiles from different clients as required.
 */

#ifndef TILE_HANDLER_HPP_
#define TILE_HANDLER_HPP_

#include  "text_popup.hpp"

namespace LinksRouting
{
namespace SlotType
{

  class TileHandler
  {
    public:
      virtual ~TileHandler() {}
      virtual bool updateRegion( TextPopup::Popup& popup,
                                 float2 center = float2(-9999, -9999),
                                 float2 rel_pos = float2() ) = 0;
      virtual bool updateRegion( XRayPopup::HoverRect& popup ) = 0;
  };

} // namespace SlotType
} // namespace LinksRouting


#endif /* TILE_HANDLER_HPP_ */
