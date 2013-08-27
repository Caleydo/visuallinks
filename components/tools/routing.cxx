/*
 * routing.cxx
 *
 *  Created on: Aug 27, 2013
 *      Author: tom
 */

#include "routing.h"

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  bool
  Routing::cmp_by_angle::operator()
  (
    Routing::segment_iterator const& lhs,
    Routing::segment_iterator const& rhs
  ) const
  {
    return getAngle(lhs->trail) < getAngle(rhs->trail);
  }

  //----------------------------------------------------------------------------
  float Routing::cmp_by_angle::getAngle(LinkDescription::points_t const& trail)
  {
    if( trail.size() < 2 )
      return 0;
    const float2 dir = trail.at(1) - trail.at(0);
    return std::atan2(dir.y, dir.x);
  }

} // namespace LinksRouting
