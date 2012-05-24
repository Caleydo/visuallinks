#ifndef LR_LINKDESCRIPTION
#define LR_LINKDESCRIPTION

#include "color.h"
#include "datatypes.h"
#include "float2.hpp"

#include <list>
#include <vector>

namespace LinksRouting
{
namespace LinkDescription
{
  class HyperEdge;
  struct HyperEdgeDescriptionSegment;
  struct HyperEdgeDescriptionForkation;

  typedef std::map<std::string, std::string> props_t;

  class Node
  {
    public:

      explicit Node( const std::vector<float2>& points,
                     const props_t& props = props_t() ):
        _points( points ),
        _props( props )
      {}

      const std::vector<float2>& getVertices() const { return _points; }
      const props_t& getProps() const { return _props; }

//        virtual float positionDistancePenalty() const = 0;
//        virtual bool hasFixedColor(Color &color) const = 0;
//        virtual const HyperEdge* getParent() const = 0;
//        virtual const std::vector<HyperEdge*>& getChildren() const = 0;
//
//        virtual void setComputedPosition(const float2& pos) = 0;
//        virtual float2 getComputedPosition() const = 0;

    private:

      std::vector<float2>  _points;
      std::map<std::string, std::string> _props;
  };

  class HyperEdge
  {
    public:

      explicit HyperEdge( const std::vector<Node>& nodes,
                          const props_t& props = props_t() ):
        _nodes( nodes ),
        _props( props ),
        _revision( 0 )
      {}

      const std::vector<Node>& getNodes() const { return _nodes; }
      const props_t& getProps() const { return _props; }
      uint32_t getRevision() const { return _revision; }

      void addNodes( const std::vector<Node>& nodes )
      {
        if( nodes.empty() )
          return;

        _nodes.insert(_nodes.end(), nodes.begin(), nodes.end());
        ++_revision;
      }

//      virtual bool hasFixedColor(Color &color) const = 0;
//      virtual const std::vector<Node*>& getConnections() const = 0;
//
        void setHyperEdgeDescription(HyperEdgeDescriptionForkation* desc)
        {
          fork = desc;
        }
        const HyperEdgeDescriptionForkation* getHyperEdgeDescription() const
        {
          return fork;
        }

    private:

      std::vector<Node> _nodes;
      props_t _props;
      uint32_t _revision; ///!< Track modifications
      HyperEdgeDescriptionForkation *fork;
  };

  struct HyperEdgeDescriptionForkation
  {
      HyperEdgeDescriptionForkation() : parent(0)
      {
      }

      std::vector<const Node*> incomingNodes;
      float2 position;

      HyperEdgeDescriptionSegment* parent;
      std::list<HyperEdgeDescriptionSegment> outgoing;
  };
  struct HyperEdgeDescriptionSegment
  {
      HyperEdgeDescriptionSegment() : parent(0), child(0)
      {
      }
      std::vector<const Node*> nodes;
      std::vector<float2> trail;

      HyperEdgeDescriptionForkation *parent, *child;
  };

  struct LinkDescription
  {
    LinkDescription( const std::string& id,
                     uint32_t stamp,
                     const HyperEdge& link,
                     uint32_t color_id ):
      _id( id ),
      _stamp( stamp ),
      _link( link ),
      _color_id( color_id )
    {}

    const std::string   _id;
    uint32_t            _stamp;
    HyperEdge           _link;
    uint32_t            _color_id;
  };

  typedef std::list<LinkDescription> LinkList;

} // namespace LinkDescription
} // namespace LinksRouting

#endif //LR_LINKDESCRIPTION
