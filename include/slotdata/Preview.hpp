/*
 * Preview.hpp
 *
 *  Created on: Oct 22, 2014
 *      Author: tom
 */

#ifndef PREVIEW_HPP_
#define PREVIEW_HPP_

#include  "text_popup.hpp"

namespace LinksRouting
{
namespace SlotType
{

  class Preview
  {
    public:
      virtual ~Preview() {}
      virtual PreviewWindow* getWindow( TextPopup::Popup*,
                                        uint8_t dev_id = 0 ) = 0;
  };

} // namespace SlotType
} // namespace LinksRouting



#endif /* PREVIEW_HPP_ */
