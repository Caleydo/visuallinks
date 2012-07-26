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
  class Node;
  class HyperEdge;
  struct HyperEdgeDescriptionSegment;
  struct HyperEdgeDescriptionForkation;

  typedef std::vector<float2> points_t;
  typedef std::map<std::string, std::string> props_t;
  typedef std::vector<Node> nodes_t;

  class Node
  {
    public:

      explicit Node( const points_t& points,
                     const props_t& props = props_t() ):
        _points( points ),
        _props( props )
      {}

      points_t& getVertices() { return _points; }
      const points_t& getVertices() const { return _points; }

      props_t& getProps() { return _props; }
      const props_t& getProps() const { return _props; }

//        virtual float positionDistancePenalty() const = 0;
//        virtual bool hasFixedColor(Color &color) const = 0;
//        virtual const HyperEdge* getParent() const = 0;
//        virtual const std::vector<HyperEdge*>& getChildren() const = 0;
//
//        virtual void setComputedPosition(const float2& pos) = 0;
//        virtual float2 getComputedPosition() const = 0;

    private:

      points_t  _points;
      props_t   _props;
  };

  class HyperEdge
  {
    public:

      explicit HyperEdge( const nodes_t& nodes,
                          const props_t& props = props_t() ):
        _nodes( nodes ),
        _props( props ),
        _revision( 0 ),
        _fork( 0 )
      {}

      nodes_t& getNodes() { return _nodes; }
      const nodes_t& getNodes() const { return _nodes; }

      props_t& getProps() { return _props; }
      const props_t& getProps() const { return _props; }

      uint32_t getRevision() const { return _revision; }

      void addNodes( const nodes_t& nodes )
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
        _fork = desc;
      }
      const HyperEdgeDescriptionForkation* getHyperEdgeDescription() const
      {
        return _fork;
      }

    private:

      nodes_t _nodes;
      props_t _props;
      uint32_t _revision; ///!< Track modifications
      HyperEdgeDescriptionForkation *_fork;
  };

  struct HyperEdgeDescriptionForkation
  {
      HyperEdgeDescriptionForkation():
        position(0,0),
        parent(0)
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
      points_t trail;

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
