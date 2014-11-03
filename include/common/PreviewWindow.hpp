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

      virtual void update(double dt) = 0;
      virtual void release() = 0;

    protected:
      Popup *_popup;

      PreviewWindow(Popup* popup):
        _popup(popup)
      {}

      virtual ~PreviewWindow() = 0;
  };

}

#endif /* LINKS_COMMON_PREVIEWWINDOW_HPP_ */
