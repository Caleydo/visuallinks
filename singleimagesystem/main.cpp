
#include "staticcore.h"
#include "xmlconfig.h"
#include "glcostanalysis.h"
#include "cpurouting.h"
#include "glrenderer.h"

using namespace LinksRouting;
int main()
{
  StaticCore core;
  XmlConfig config;
  GlCostAnalysis costanalysis;
  CPURouting cpurouting;
  GlRenderer glrenderer;
  core.startup("config.xml");
  core.attachComponent(&config);
  core.attachComponent(&costanalysis);
  core.attachComponent(&cpurouting);
  core.attachComponent(&glrenderer);
  core.init();
  core.process();
  core.shutdown();
  return 0;
}