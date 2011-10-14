#ifndef LR_LINKDESCRIPTION
#define LR_LINKDESCRIPTION
#include <vector>
#include <color.h>

namespace LinksRouting
{
  namespace LinkDescription
  {
    class Node;
    class HyperEdge;
    struct HyperEdgeDescriptionSegment;
    struct HyperEdgeDescriptionForkation;
    struct Point
    {
      float x;
      float y;
    };

    class Node
    {
    public:
      virtual const std::vector<Point>& getVertices() const = 0;
      virtual float positionDistancePenalty() const = 0;
      virtual bool hasFixedColor(Color &color) const = 0;
      virtual const HyperEdge* getParent() const = 0;
      virtual const std::vector<HyperEdge*>& getChildren() const = 0;

      virtual void setComputedPosition(const Point& pos) = 0;
      virtual Point getComputedPosition() const = 0;
    };
    class HyperEdge
    {
    public:
      virtual bool hasFixedColor(Color &color) const = 0;
      virtual const std::vector<Node*>& getConnections() const = 0;

      virtual void setHyperEdgeDescription(HyperEdgeDescriptionForkation* desc) = 0;
      virtual const HyperEdgeDescriptionForkation* getHyperEdgeDescription() const = 0;
    };

    struct HyperEdgeDescriptionForkation
    {
      std::vector<Node*> incomingNodes;
      Point position;

      HyperEdgeDescriptionSegment* parent;
      std::vector<HyperEdgeDescriptionSegment*> outgoing;
    };
    struct HyperEdgeDescriptionSegment
    {
      std::vector<Node*> nodes;
      std::vector<Point> trail;
      HyperEdgeDescriptionSegment *parent, *child;
    };

  };
};


#endif //LR_LINKDESCRIPTION
