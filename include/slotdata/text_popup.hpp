/*
 * text_popup.hpp
 *
 *  Created on: 20.09.2012
 *      Author: tom
 */

#ifndef TEXT_POPUP_HPP_
#define TEXT_POPUP_HPP_

#include "../float2.hpp"

#include <string>
#include <vector>

namespace LinksRouting
{
namespace SlotType
{

  struct TextPopup
  {
    struct Popup
    {
      std::string text;
      float2 pos;
      float2 size;
      bool visible;
    };

    typedef std::vector<Popup> Popups;
    Popups popups;
  };

} // namespace SlotType
} // namespace LinksRouting


#endif /* TEXT_POPUP_HPP_ */
