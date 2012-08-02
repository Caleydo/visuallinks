#ifndef LR_ROUTING
#define LR_ROUTING
#include <component.h>
#include <linkdescription.h>
#include <map>

namespace LinksRouting
{
  class CostAnalysis;
  class Renderer;
  class Routing : public virtual Component
  {
    protected:

      typedef std::map<LinkDescription::Node*, size_t> LevelNodeMap;
      typedef std::map<LinkDescription::HyperEdge*, size_t> LevelHyperEdgeMap;

      /**
       * There can be loops due to hyperedges connecting the same nodes, so we
       * have to avoid double entries the same way, one node could be put on
       * different levels, so we need to analyse the nodes level first
       */
      void checkLevels( LevelNodeMap& levelNodeMap,
                        LevelHyperEdgeMap& levelHyperEdgeMap,
                        LinkDescription::HyperEdge& hedge,
                        size_t level = 0 )
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
          if( node->get<bool>("hidden", false) )
            continue;
          auto found = levelNodeMap.find(&*node);
          if(found != levelNodeMap.end())
            found->second = std::max(found->second,level);
          else
            levelNodeMap.insert(std::make_pair(&*node, level));
          //has child hyperedges
          for( auto it = node->getChildren().begin();
                    it != node->getChildren().end();
                  ++it )
            checkLevels(levelNodeMap, levelHyperEdgeMap, *(*it), level + 1);
        }
      }
  };
};


#endif //LR_ROUTING
