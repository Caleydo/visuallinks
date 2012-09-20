/*!
 * @file mouse_event.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 21.08.2012
 */

#ifndef _SLOTDATA_MOUSE_EVENT_HPP_
#define _SLOTDATA_MOUSE_EVENT_HPP_

#include <functional>
#include <vector>

namespace LinksRouting
{
namespace SlotType
{

  struct MouseEvent
  {
    typedef std::function<void (int, int)> MoveCallback;
    typedef std::function<void ()> LeaveCallback;

    std::vector<MoveCallback> _move_callbacks;
    std::vector<LeaveCallback> _leave_callbacks;
    
    void triggerMove(int x, int y)
    {
      for( auto cb = _move_callbacks.begin();
                cb != _move_callbacks.end();
              ++cb )
        (*cb)(x, y);
    }
    
    void triggerLeave()
    {
      for( auto cb = _leave_callbacks.begin();
                cb != _leave_callbacks.end();
              ++cb )
        (*cb)();
    }

    void clear()
    {
      _move_callbacks.clear();
      _leave_callbacks.clear();
    }
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _SLOTDATA_MOUSE_EVENT_HPP_ */
