#ifndef LR_LINKDESCRIPTION
#define LR_LINKDESCRIPTION

#include "color.h"
#include "datatypes.h"
#include "float2.hpp"
#include "string_utils.h"

#include <list>
#include <map>
#include <vector>
#include <memory>

namespace LinksRouting
{
namespace LinkDescription
{
  class HyperEdge;
  typedef std::shared_ptr<HyperEdge> HyperEdgePtr;
  struct HyperEdgeDescriptionSegment;
  struct HyperEdgeDescriptionForkation;
  typedef std::shared_ptr<HyperEdgeDescriptionForkation>
          HyperEdgeDescriptionForkationPtr;
  typedef std::shared_ptr<const HyperEdgeDescriptionForkation>
          HyperEdgeDescriptionForkationConstPtr;

  typedef std::vector<float2> points_t;
  typedef std::map<std::string, std::string> props_t;
  typedef std::vector<HyperEdgePtr> hedges_t;

  class PropertyElement
  {
    public:
      props_t& getProps() { return _props; }
      const props_t& getProps() const { return _props; }

      /**
       * Set a property
       *
       * @param key     Property key
       * @param val     New value
       */
      template<typename T>
      void set(const std::string& key, const T& val)
      {
//#if defined(WIN32) || defined(_WIN32)
        // Use stringstream because visual studio seems do not have the proper
        // std::to_string overloads.
		std::stringstream strm;
		strm << val;
		_props[ key ] = strm.str();
//#else
//		_props[ key ] = std::to_string(val);
//#endif
      }

//      void set(const std::string& key, const std::string& val)
//      {
//        _props[ key ] = val;
//      }
//
//      void set(const std::string& key, const char* val)
//      {
//        _props[ key ] = val;
//      }

      /**
       * Get a property
       *
       * @param key     Property key
       * @param def_val Default value if property doesn't exist
       * @return
       */
      template<typename T>
      T get(const std::string& key, const T& def_val = T()) const
      {
        props_t::const_iterator it = _props.find(key);
        if( it == _props.end() )
          return def_val;

        return convertFromString(it->second, def_val);
      }

    protected:
      props_t   _props;

      PropertyElement( const props_t& props = props_t() ):
        _props( props )
      {}
  };

  class Node:
    public PropertyElement
  {
    friend class HyperEdge;
    public:

      Node();
      explicit Node( const points_t& points,
                     const props_t& props = props_t() );
      Node( const points_t& points,
            const points_t& link_points,
            const props_t& props = props_t() );
      Node( const points_t& points,
            const points_t& link_points,
            const points_t& link_points_children,
            const props_t& props = props_t() );
      explicit Node(HyperEdgePtr hedge);

      points_t& getVertices();
      const points_t& getVertices() const;

      points_t& getLinkPoints();
      const points_t& getLinkPoints() const;

      points_t& getLinkPointsChildren();
      const points_t& getLinkPointsChildren() const;

      float2 getCenter() const;
      Rect getBoundingBox() const;

      HyperEdge* getParent();
      const HyperEdge* getParent() const;

      const hedges_t& getChildren() const;
      hedges_t& getChildren();

      void addChildren(const hedges_t& edges);
      void addChild(const HyperEdgePtr& hedge);

    private:

      points_t _points;
      points_t _link_points;
      points_t _link_points_children;
      HyperEdge* _parent;
      hedges_t _children;
  };

  typedef std::shared_ptr<Node> NodePtr;
  typedef std::list<NodePtr> nodes_t;

  class HyperEdge:
    public PropertyElement
  {
    friend class Node;
    public:

      HyperEdge();
      explicit HyperEdge( const nodes_t& nodes,
                          const props_t& props = props_t() );

      nodes_t& getNodes();
      const nodes_t& getNodes() const;

      void setCenter(const float2& center);
      const float2& getCenter() const;

      uint32_t getRevision() const;

      void addNodes(const nodes_t& nodes);
      void addNode(const NodePtr& node);
      void resetNodeParents();

      const Node* getParent() const;

      void setHyperEdgeDescription(HyperEdgeDescriptionForkationPtr desc);
      HyperEdgeDescriptionForkationPtr getHyperEdgeDescription();
      HyperEdgeDescriptionForkationConstPtr getHyperEdgeDescription() const;

      void removeRoutingInformation();

    private:

      HyperEdge(const HyperEdge&) /* = delete */;
      HyperEdge& operator=(const HyperEdge&) /* = delete */;

      Node* _parent;
      nodes_t _nodes;
      float2 _center;     ///!< Estimated center
      uint32_t _revision; ///!< Track modifications
      HyperEdgeDescriptionForkationPtr _fork;
  };


  struct HyperEdgeDescriptionSegment
  {
      nodes_t nodes;
      points_t trail;
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
                     const HyperEdgePtr& link,
                     uint32_t color_id ):
      _id( id ),
      _stamp( stamp ),
      _link( link ),
      _color_id( color_id )
    {}

    const std::string   _id;
    uint32_t            _stamp;
    HyperEdgePtr        _link;
    uint32_t            _color_id;
  };

  typedef std::list<LinkDescription> LinkList;

} // namespace LinkDescription
} // namespace LinksRouting

#endif //LR_LINKDESCRIPTION
