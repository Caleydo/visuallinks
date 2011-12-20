/*!
 * @file slots.cpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 16.12.2011
 */

#include "slots.hpp"

#include <iostream>

namespace LinksRouting
{

  //------------------------------------------------------------------------------
  internal::Slot::~Slot()
  {

  }

  //----------------------------------------------------------------------------
  bool internal::Slot::isValid() const
  {
    return _valid;
  }

  //----------------------------------------------------------------------------
  void internal::Slot::setValid(bool val)
  {
    _valid = val;
  }

  //----------------------------------------------------------------------------
  SlotCollector::SlotCollector(slots_t& slots):
    _slots( slots )
  {

  }

  //----------------------------------------------------------------------------
  void SlotCollector::appendSlot(const std::string& name, slot_ref_t slot)
  {
    if( _slots.find(name) != _slots.end() )
      std::cerr << "Slot (" << name << ") already exists - overwriting..."
                << std::endl;

    _slots[name] = slot;
  }

  //----------------------------------------------------------------------------
  SlotSubscriber::SlotSubscriber(slots_t& slots):
    _slots( slots )
  {

  }
} // namespace LinksRouting
