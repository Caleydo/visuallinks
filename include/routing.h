#ifndef LR_ROUTING
#define LR_ROUTING
#include <component.h>
#include <linkdescription.h>
#include <map>
#include <set>

namespace LinksRouting
{
  class CostAnalysis;
  class Renderer;
  class Routing:
    public virtual Component
  {
    public:

      typedef LinkDescription::HedgeSegmentList::iterator segment_iterator;
      typedef std::vector<segment_iterator> SegmentIterators;

      /**
       * Compare hedge segments by angle
       */
      struct cmp_by_angle
      {
        bool operator()( segment_iterator const& lhs,
                         segment_iterator const& rhs ) const;
        static float getAngle(LinkDescription::points_t const& trail);
      };

      typedef std::set<segment_iterator, cmp_by_angle> OrderedSegments;

    protected:

      Routing();
      void subdivide(LinkDescription::points_t& trail) const;

      /**
       * Smooth a line given by a list of points. The more iterations you choose
       * the smoother the curve will become. How smooth it can get depends on
       * the number of input points, as no new points will be added.
       *
       * @param points
       * @param smoothing_factor
       * @param iterations
       */
      template<typename Collection>
      static std::vector<float2> smooth( const Collection& points,
                                         float smoothing_factor,
                                         unsigned int iterations );

      typedef std::map<LinkDescription::Node*, size_t> LevelNodeMap;
      typedef std::map<LinkDescription::HyperEdge*, size_t> LevelHyperEdgeMap;

#if 0
      /**
       * There can be loops due to hyperedges connecting the same nodes, so we
       * have to avoid double entries the same way, one node could be put on
       * different levels, so we need to analyse the nodes level first
       */
      void checkLevels( LevelNodeMap& levelNodeMap,
                        LevelHyperEdgeMap& levelHyperEdgeMap,
                        LinkDescription::HyperEdge& hedge,
                        size_t level = 0 );
#endif
  };

  template<typename Collection>
  std::vector<float2> Routing::smooth( const Collection& points,
                                       float smoothing_factor,
                                       unsigned int iterations )

  {
    // internal helper...
    auto smoothIteration = []( const std::vector<float2> in,
                               std::vector<float2>& out,
                               float smoothing_factor )
    {
      auto begin = in.begin(),
           end = in.end();

      decltype(begin) p0 = begin,
                      p1 = p0 + 1,
                      p2 = p1 + 1;

      float f_other = 0.5f*smoothing_factor;
      float f_this = 1.0f - smoothing_factor;

      out.push_back(*p0);

      for(; p2 != end; ++p0, ++p1, ++p2)
        out.push_back(f_other * (*p0 + *p2) + f_this * (*p1));

      out.push_back(*p1);
    };

    // now it's time to use a specific container...
    std::vector<float2> in(std::begin(points), std::end(points));

    if( in.size() < 3 )
      return in;

    std::vector<float2> out;
    out.reserve(in.size());

    smoothIteration( in, out, smoothing_factor );

    for(unsigned int step = 1; step < iterations; ++step)
    {
      in.swap(out);
      out.clear();
      smoothIteration(in, out, smoothing_factor);
    }

    return out;
  }
}


#endif //LR_ROUTING
