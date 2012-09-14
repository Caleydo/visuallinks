#ifndef LR_COMPONENT
#define LR_COMPONENT

#include "configurable.h"
#include "slots.hpp"

namespace LinksRouting
{
  class Core;
  class SlotCollector;
  class Component:
    public virtual Configurable
  {
    public:

      enum Type
      {
        None = 0,
        Config = 1,
        DataServer = 2,
        TransparencyAnalysis = 4,
        Costanalysis = 8,
        Routing = 16,
        Renderer = 32,
        Any = 63
      };

      static std::string TypeToString(Type t)
      {
        switch( t )
        {
          case Config:
            return "ComponentConfig";
          case DataServer:
            return "DataServer";
          case TransparencyAnalysis:
            return "ComponentTransparencyAnalysis";
          case Costanalysis:
            return "ComponentCostanalysis";
          case Renderer:
            return "ComponentRenderer";
          case Routing:
            return "ComponentRouting";
          case Any:
            return "ComponentAny";
          default:
            return "Unknown";
        }
      }
      static Type StringToType(const std::string & str)
      {
        if( str == "ComponentConfig" )
          return Config;
        else if( str == "ComponentDataServer" )
          return DataServer;
        else if( str == "ComponentTransparencyAnalysis" )
          return TransparencyAnalysis;
        else if( str == "ComponentCostanalysis" )
          return Costanalysis;
        else if( str == "ComponentRenderer" )
          return Renderer;
        else if( str == "ComponentRouting" )
          return Routing;
        else if( str == "ComponentAny" )
          return Any;
        return None;
      }

      /**
       * Override to publish data to slots
       */
      virtual void publishSlots(SlotCollector& slot_list) {};
      virtual void subscribeSlots(SlotSubscriber& slot_subscriber) {};

      virtual bool startup(Core* core, unsigned int type) { return true; };
      virtual void init() {};
      virtual bool initGL() { return true; };
      virtual void shutdown() {};
      virtual bool supports(unsigned int type) const { return false; };

      virtual void process(unsigned int type) {};

    protected:

      Component():
        Configurable("UnnamedComponent")
      {}
  };

}

#endif //LR_COMPONENT
