#include "glrenderer.h"
#include <iostream>

namespace LinksRouting
{

  GlRenderer::GlRenderer() : myname("GlRenderer")
  {
    //DUMMYTEST
    registerArg("TestDouble", TestDouble);
    registerArg("TestString", TestString);
  }

  bool GlRenderer::startup(Core* core, unsigned int type)
  {

    return true;
  }
  void GlRenderer::init()
  {

  }
  void GlRenderer::shutdown()
  {

  }

  void GlRenderer::process(Type type)
  {
    //DUMMYTEST
    std::cout << "TestDouble: " << TestDouble << " TestString: " << TestString << std::endl;
  }

  bool GlRenderer::setTransparencyInput(const Component::MapData& inputmap)
  {
    return true;
  }
  bool GlRenderer::addLinkHierarchy(LinkDescription::Node* node)
  {
    return true;
  }
  bool GlRenderer::addLinkHierarchy(LinkDescription::HyperEdge* hyperedge)
  {
    return true;
  }
  bool GlRenderer::removeLinkHierarchy(LinkDescription::Node* node)
  {
    return true;
  }
  bool GlRenderer::removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge)
  {
    return true;
  }
};