/*!
 * @file mouse_event.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 21.08.2012
 */

#ifndef _SLOTDATA_MOUSE_EVENT_HPP_
#define _SLOTDATA_MOUSE_EVENT_HPP_

#include "float2.hpp"

#include <functional>
#include <vector>
#include <stdint.h>

namespace LinksRouting
{
namespace SlotType
{

  struct MouseEvent
  {
    /**
     * Enum values from QtCore/qtnamespace.h
     */
    enum KeyboardModifier {
      NoModifier           = 0x00000000,//!< NoModifier
      ShiftModifier        = 0x02000000,//!< ShiftModifier
      ControlModifier      = 0x04000000,//!< ControlModifier
      AltModifier          = 0x08000000,//!< AltModifier
      MetaModifier         = 0x10000000,//!< MetaModifier
      KeypadModifier       = 0x20000000,//!< KeypadModifier
      GroupSwitchModifier  = 0x40000000,//!< GroupSwitchModifier
      // Do not extend the mask to include 0x01000000
      KeyboardModifierMask = 0xfe000000 //!< KeyboardModifierMask
    };

    typedef std::function<void (int, int)> ClickCallback;
    typedef std::function<void (int, int)> MoveCallback;
    typedef std::function<void (const float2&)> DragCallback;
    typedef std::function<void ()> LeaveCallback;
    typedef std::function<void (int, const float2&, uint32_t)> ScrollCallback;

    std::vector<ClickCallback> _click_callbacks;
    std::vector<MoveCallback> _move_callbacks;
    std::vector<DragCallback> _drag_callbacks;
    std::vector<LeaveCallback> _leave_callbacks;
    std::vector<ScrollCallback> _scroll_callbacks;
    
    void triggerClick(int x, int y)
    {
      for( auto cb = _click_callbacks.begin();
                cb != _click_callbacks.end();
              ++cb )
        (*cb)(x, y);
    }

    void triggerMove(int x, int y)
    {
      for( auto cb = _move_callbacks.begin();
                cb != _move_callbacks.end();
              ++cb )
        (*cb)(x, y);
    }
    
    void triggerDrag(const float2& d)
    {
      for( auto cb = _drag_callbacks.begin();
                cb != _drag_callbacks.end();
              ++cb )
        (*cb)(d);
    }

    void triggerLeave()
    {
      for( auto cb = _leave_callbacks.begin();
                cb != _leave_callbacks.end();
              ++cb )
        (*cb)();
    }

    void triggerScroll(int delta, const float2& pos, uint32_t mod)
    {
      for( auto cb = _scroll_callbacks.begin();
                cb != _scroll_callbacks.end();
              ++cb )
        (*cb)(delta, pos, mod);
    }

    void clear()
    {
      _click_callbacks.clear();
      _move_callbacks.clear();
      _drag_callbacks.clear();
      _leave_callbacks.clear();
      _scroll_callbacks.clear();
    }
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _SLOTDATA_MOUSE_EVENT_HPP_ */
