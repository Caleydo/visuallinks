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
#include <vector>

namespace LinksRouting
{
namespace SlotType
{

  struct ComponentSelection
  {
    ComponentSelection():
      active(0)
    {}

    std::vector<std::string> available;
    int active;
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _COMPONENT_SELECTION_HPP_ */
