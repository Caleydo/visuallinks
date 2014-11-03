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
#include <stdexcept>

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

        //C++10 not yet fully supported...
        //Slot() = default;
        Slot(): _valid(false) { }

        virtual ~Slot() = 0;

        /**
         * Get validity of current slot data
         */
        bool isValid() const;

        /**
         * Set validity
         */
        void setValid(bool val);


    private:
        // No slot copying
        //C++10 not yet fully supported...
        Slot(const Slot&);// = delete;
        const Slot& operator=(const Slot&);// = delete;

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
      typedef std::unique_ptr<DataType> StorageRef;
      StorageRef _data;

      template<class T = DataType>
      static std::shared_ptr<Slot> make_slot()
      {
        return std::shared_ptr<Slot>(new Slot(new T));
      }

      template<class A1, class T = DataType>
      static std::shared_ptr<Slot> make_slot(A1 const& arg)
      {
        return std::shared_ptr<Slot>(new Slot(new T(arg)));
      }

    private:

      explicit Slot(DataType* d):
        _data(d)
      {}
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
      template<typename PtrType, typename DataType = PtrType>
      typename slot_t<PtrType>::type create(const std::string& name)
      {
        typedef typename slot_t<PtrType>::raw_type slot_type;
        auto slot = slot_type::template make_slot<DataType>();
        appendSlot(name, slot);

        return slot;
      }

      template<typename PtrType, typename DataType, typename A1>
      typename slot_t<PtrType>::type create( const std::string& name,
                                             A1 const& arg )
      {
        typedef typename slot_t<PtrType>::raw_type slot_type;
        auto slot = slot_type::template make_slot<A1, DataType>(arg);
        appendSlot(name, slot);

        return slot;
      }

    private:

      void appendSlot(const std::string& name, slot_ref_t slot);

      slots_t   &_slots;
  };

  /**
   * A wrapper around a slot list which only allows subscribing slots.
   */
  class SlotSubscriber
  {
    public:

      SlotSubscriber(slots_t& slots);

      /**
       * Get the Slot with the given name.
       */
      template<typename DataType>
      typename slot_t<DataType>::type getSlot(const std::string& name)// const
      {
        auto slot = _slots.find(name);

        if( slot == _slots.end() )
          throw std::runtime_error("No such slot: " + name);

        auto casted_slot =
          std::dynamic_pointer_cast<typename slot_t<DataType>::raw_type>(slot->second.lock());

        if( !casted_slot )
          throw std::runtime_error("Wrong slot type for slot: " + name);

        return casted_slot;
      }

    private:

      slots_t   &_slots;
  };

} // namespace LinksRouting


#endif /* _SLOTS_HPP_ */
