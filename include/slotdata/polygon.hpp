/*!
 * @file polygon.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 12.03.2012
 */

#ifndef _POLYGON_HPP_
#define _POLYGON_HPP_

#include <vector>

namespace LinksRouting
{
namespace SlotType
{

  struct Polygon
  {
    struct Point
    {
      int x, y;
      Point(int _x, int _y) : x(_x), y(_y) { }
    };

    std::vector<Point> points;
  };

} // namespace SlotType
} // namespace LinksRouting

#endif /* _POLYGON_HPP_ */
