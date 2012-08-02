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
    friend class HyperEdge;
    public:

      explicit Node( const std::vector<float2>& points,
                     const props_t& props = props_t() ):
        _points( points ),
        _props( props ),
        _parent(0)
      {}

      const std::vector<float2>& getVertices() const { return _points; }
      std::vector<float2>& getVertices() { return _points; }
      const props_t& getProps() const { return _props; }


      HyperEdge* getParent()
      {
        return _parent;
      }
      const HyperEdge* getParent() const
      {
        return _parent;
      }
      const std::vector<HyperEdge*>& getChildren() const
      {
        return _children;
      }
      std::vector<HyperEdge*>& getChildren()
      {
        return _children;
      }
      inline void addChildren(std::vector<HyperEdge*>& edges);


//        virtual float positionDistancePenalty() const = 0;
//        virtual bool hasFixedColor(Color &color) const = 0;
//
//        virtual void setComputedPosition(const float2& pos) = 0;
//        virtual float2 getComputedPosition() const = 0;

    private:

      std::vector<float2>  _points;
      std::map<std::string, std::string> _props;
      HyperEdge* _parent;
      std::vector<HyperEdge*> _children;
  };

  class HyperEdge
  {
    friend class Node;
    public:

      explicit HyperEdge( const std::vector<Node>& nodes,
                          const props_t& props = props_t() ):
        _parent(0),
        _nodes( nodes ),
        _props( props ),
        _revision( 0 ),
        _fork( 0 )
      {}

      const std::vector<Node>& getNodes() const { return _nodes; }
      std::vector<Node>& getNodes()  { return _nodes; }
      const props_t& getProps() const { return _props; }
      uint32_t getRevision() const { return _revision; }

      void addNodes( std::vector<Node>& nodes )
      {
        if( nodes.empty() )
          return;
        for(auto it = nodes.begin(); it != nodes.end(); ++it)
          it->_parent = this;
        _nodes.insert(_nodes.end(), nodes.begin(), nodes.end());
        ++_revision;
      }

      const Node* getParent() const
      {
        return _parent;
      }

//      virtual bool hasFixedColor(Color &color) const = 0;
//      virtual const std::vector<Node*>& getConnections() const = 0;
//
        void setHyperEdgeDescription(HyperEdgeDescriptionForkation* desc)
        {
          _fork = desc;
        }
        HyperEdgeDescriptionForkation* getHyperEdgeDescription()
        {
          return _fork;
        }
        const HyperEdgeDescriptionForkation* getHyperEdgeDescription() const
        {
          return _fork;
        }

        inline void removeRoutingInformation();

    private:

      Node* _parent;
      std::vector<Node> _nodes;
      props_t _props;
      uint32_t _revision; ///!< Track modifications
      HyperEdgeDescriptionForkation *_fork;
  };


  struct HyperEdgeDescriptionSegment
  {
      std::vector<const Node*> nodes;
      std::vector<float2> trail;
  };

  struct HyperEdgeDescriptionForkation
  {
      HyperEdgeDescriptionForkation()
      {
      }

      float2 position;

      HyperEdgeDescriptionSegment incoming;
      std::list<HyperEdgeDescriptionSegment> outgoing;
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


  void Node::addChildren(std::vector<HyperEdge*>& edges)
  {
    for(auto it = edges.begin(); it != edges.end(); ++it)
      (*it)->_parent = this;
    _children.insert(_children.end(), edges.begin(), edges.end());
  }

  void HyperEdge::removeRoutingInformation()
  {
    if(_fork) delete _fork;
    _fork = 0;
  }

} // namespace LinkDescription
} // namespace LinksRouting

#endif //LR_LINKDESCRIPTION
