#include "glcostanalysis.h"

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  GlCostAnalysis::GlCostAnalysis():
    myname("GlCostAnalysis")
  {

  }

  //----------------------------------------------------------------------------
  GlCostAnalysis::~GlCostAnalysis()
  {

  }

  //----------------------------------------------------------------------------
  bool GlCostAnalysis::startup(Core* core, unsigned int type)
  {
    return true;
  }
  void GlCostAnalysis::init()
  {

  }
  void GlCostAnalysis::shutdown()
  {

  }

  void GlCostAnalysis::process(Type type)
  {

  }

  bool GlCostAnalysis::setSceneInput(const Component::MapData& inputmap)
  {
    return true;
  }
  bool GlCostAnalysis::setCostreductionInput(const Component::MapData& inputmap)
  {
    return true;
  }
  void GlCostAnalysis::connect(LinksRouting::Routing* routing)
  {

  }

  void GlCostAnalysis::computeColorCostMap(const Color& c)
  {

  }
};
