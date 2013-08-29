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

  //----------------------------------------------------------------------------
  Routing::Routing():
   Configurable("Routing")
  {

  }

  //----------------------------------------------------------------------------
  void Routing::subdivide(LinkDescription::points_t& trail) const
  {
    size_t old_size = trail.size();
    trail.resize(old_size * 2 - 1);

    for(size_t i = old_size - 1; i > 0; --i)
    {
      trail[i * 2    ] = trail[i];
      trail[i * 2 - 1] = 0.5 * (trail[i] + trail[i - 1]);
    }
  }

#if 0
  //----------------------------------------------------------------------------
  void Routing::checkLevels( LevelNodeMap& levelNodeMap,
                             LevelHyperEdgeMap& levelHyperEdgeMap,
                             LinkDescription::HyperEdge& hedge,
                             size_t level )
  {
    auto found = levelHyperEdgeMap.find(&hedge);
    if(found != levelHyperEdgeMap.end())
      found->second = std::max(found->second,level);
    else
      levelHyperEdgeMap.insert(std::make_pair(&hedge, level));
    ++level;
    for( auto node = hedge.getNodes().begin();
              node != hedge.getNodes().end();
            ++node )
    {
      if( (*node)->get<bool>("hidden", false) )
        continue;
      auto found = levelNodeMap.find(node->get());
      if(found != levelNodeMap.end())
        found->second = std::max(found->second,level);
      else
        levelNodeMap.insert(std::make_pair(node->get(), level));
      //has child hyperedges
      for( auto it = (*node)->getChildren().begin();
                it != (*node)->getChildren().end();
              ++it )
        checkLevels(levelNodeMap, levelHyperEdgeMap, *(*it), level + 1);
    }
  }
#endif

} // namespace LinksRouting
