/*!
 * @file component_selection.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 20.07.2012
 */

#ifndef _COMPONENT_SELECTION_HPP_
#define _COMPONENT_SELECTION_HPP_

#include <string>
#include <map>

namespace LinksRouting
{
namespace SlotType
{

  struct ComponentSelection
  {
    public:

      ComponentSelection():
        _default(0)
      {}

      /** name -> available */
      std::map<std::string, bool> available;

      /** Name of active component */
      std::string active;

      /** Name of request component (should get active) */
      std::string request;

      const std::string getDefault() const
      {
        if( _default )
          return *_default;
        return std::string();
      }

      void linkDefault(const std::string* def)
      {
        _default = def;
      }

    private:

      /** Name of default component */
      const std::string *_default;
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _COMPONENT_SELECTION_HPP_ */
