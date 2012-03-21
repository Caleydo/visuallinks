#ifndef LR_LINKDESCRIPTION
#define LR_LINKDESCRIPTION

#include "color.h"
#include "datatypes.h"

#include <list>
#include <vector>

namespace LinksRouting
{
namespace LinkDescription
{
  class HyperEdge;
  struct HyperEdgeDescriptionSegment;
  struct HyperEdgeDescriptionForkation;

  struct Point
  {
    Point()
    {
    }
    Point(int x, int y):
      x(x), y(y)
    {}
    int x, y;
  };

  typedef std::map<std::string, std::string> props_t;

  class Node
  {
    public:

      explicit Node( const std::vector<Point>& points,
                     const props_t& props = props_t() ):
        _points( points ),
        _props( props )
      {}

      const std::vector<Point>& getVertices() const { return _points; }
      const props_t& getProps() const { return _props; }

//        virtual float positionDistancePenalty() const = 0;
//        virtual bool hasFixedColor(Color &color) const = 0;
//        virtual const HyperEdge* getParent() const = 0;
//        virtual const std::vector<HyperEdge*>& getChildren() const = 0;
//
//        virtual void setComputedPosition(const Point& pos) = 0;
//        virtual Point getComputedPosition() const = 0;

    private:

      std::vector<Point>  _points;
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
      Point position;

      HyperEdgeDescriptionSegment* parent;
      std::list<HyperEdgeDescriptionSegment> outgoing;
  };
  struct HyperEdgeDescriptionSegment
  {
      HyperEdgeDescriptionSegment() : parent(0), child(0)
      {
      }
      std::vector<const Node*> nodes;
      std::vector<Point> trail;

      HyperEdgeDescriptionForkation *parent, *child;
  };

  struct LinkDescription
  {
    LinkDescription( const std::string& id,
                     uint32_t stamp,
                     const HyperEdge& link ):
      _id( id ),
      _stamp( stamp ),
      _link( link )
    {}

    const std::string   _id;
    uint32_t            _stamp;
    HyperEdge           _link;
  };

  typedef std::list<LinkDescription> LinkList;

} // namespace LinkDescription
} // namespace LinksRouting

#endif //LR_LINKDESCRIPTION
