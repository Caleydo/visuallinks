/*!
 * @file slots.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 16.12.2011
 */

#ifndef _SLOTS_HPP_
#define _SLOTS_HPP_

#include <map>
#include <string>
#include <memory>
#include <cassert>

namespace LinksRouting
{
  namespace internal
  {
    /**
     * Base class for slots to enable uniform of specialized slot templates
     */
    class Slot
    {
      public:

        Slot() = default;

        virtual ~Slot() = 0;

        /**
         * Get validity of current slot data
         */
        bool isValid() const;

        /**
         * Set validity
         */
        void setValid(bool val);

        // No slot copying
        Slot(const Slot&) = delete;
        const Slot& operator=(const Slot&) = delete;

      protected:

        bool  _valid;

    };

  } // namespace internal

  /**
   * A slot enables communication between components
   */
  template<typename DataType>
  class Slot:
    public internal::Slot
  {
    public:

      Slot():
        _data( new DataType() )
      {}

    //protected:

      std::unique_ptr<DataType> _data;

  };

  // workarround for missing template alias in visual studio...
  template<typename DataType>
  struct slot_t
  {
    typedef Slot<DataType> raw_type;
    typedef std::shared_ptr<raw_type> type;
  };

  typedef std::weak_ptr<internal::Slot> slot_ref_t;
  typedef std::map<std::string, slot_ref_t> slots_t;

  class SlotManager
  {
    private:

      slots_t _slots;
  };

  /**
   * A wrapper around a slot list which only allows adding slots.
   */
  class SlotCollector
  {
    public:

      SlotCollector(slots_t& slots);

      /**
       * Create a new Slot with the given name.
       */
      template<typename DataType>
      typename slot_t<DataType>::type create(const std::string& name)
      {
        auto slot = std::make_shared<typename slot_t<DataType>::raw_type>();
        appendSlot(name, slot);

        return slot;
      }

    private:

      void appendSlot(const std::string& name, slot_ref_t slot);

      slots_t   &_slots;
  };

} // namespace LinksRouting


#endif /* _SLOTS_HPP_ */