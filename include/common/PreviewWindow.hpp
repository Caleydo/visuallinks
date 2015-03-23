/*
 * PreviewWindow.hpp
 *
 *  Created on: Oct 22, 2014
 *      Author: tom
 */

#ifndef LINKS_COMMON_PREVIEWWINDOW_HPP_
#define LINKS_COMMON_PREVIEWWINDOW_HPP_

#include "slotdata/text_popup.hpp"

namespace LinksRouting
{

  class PreviewWindow
  {
    public:
      typedef SlotType::TextPopup::Popup Popup;
      typedef SlotType::XRayPopup::HoverRect SeeThrough;

      virtual void update(double dt) = 0;
      virtual void release() = 0;

    protected:
      Popup      *_popup;
      SeeThrough *_see_through;

      explicit PreviewWindow(Popup* popup):
        _popup(popup),
        _see_through(nullptr)
      {}

      explicit PreviewWindow(SeeThrough* see_through):
        _popup(nullptr),
        _see_through(see_through)
      {}

      virtual ~PreviewWindow() = 0;
  };

}

#endif /* LINKS_COMMON_PREVIEWWINDOW_HPP_ */
