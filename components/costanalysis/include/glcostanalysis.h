#ifndef LR_GLCOSTANALYSIS
#define LR_GLCOSTANALYSIS

#include <string>

#include <costanalysis.h>
#include <common/componentarguments.h>

namespace LinksRouting
{
  class GlCostAnalysis:
    public CostAnalysis,
    public ComponentArguments
  {
  protected:
    std::string myname;
  public:
    GlCostAnalysis();
    virtual ~GlCostAnalysis();

    bool startup(Core* core, unsigned int type);
    void init();
    void shutdown();
    bool supports(Type type) const
    {
      return type == Component::Costanalysis;
    }
    const std::string& name() const
    {
      return myname;
    }

    void process(Type type);

    bool setSceneInput(const Component::MapData& inputmap);
    bool setCostreductionInput(const Component::MapData& inputmap);
    void connect(LinksRouting::Routing* routing);

    void computeColorCostMap(const Color& c);

  };
};
#endif //LR_GLCOSTANALYSIS
