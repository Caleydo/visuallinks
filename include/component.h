#ifndef LR_COMPONENT
#define LR_COMPONENT

#include "log.hpp"
#include "slots.hpp"
#include "string_utils.h"
#include <string>

namespace LinksRouting
{
  class Core;
  class SlotCollector;
  class Component
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


      template<class Type>
      inline static std::string getDataTypeString();

      template<class Type>
      inline static bool isDataType(const std::string& type)
      {
        return getDataTypeString<Type>() == type;
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
      virtual const std::string& name() const = 0;

      virtual void process(unsigned int type) {};

      virtual bool setFlag(const std::string& name, bool val) = 0;
      virtual bool getFlag(const std::string& name, bool& val) const = 0;
      virtual bool setInteger(const std::string& name, int val) = 0;
      virtual bool getInteger(const std::string& name, int& val) const = 0;
      virtual bool setFloat(const std::string& name, double val) = 0;
      virtual bool getFloat(const std::string& name, double& val) const = 0;
      virtual bool setString(const std::string& name,
                             const std::string& val) = 0;
      virtual bool getString(const std::string& name,
                             std::string& val) const = 0;

      template<class Type>
      inline
      bool set(const std::string& key, const std::string& val)
      {
        return setImpl<Type>(this, key, val);
      }

      bool set( const std::string& key,
                const std::string& val,
                const std::string& type )
      {
        if( isDataType<bool>(type) )
          return set<bool>(key, val);
        else if( isDataType<int>(type) )
          return set<int>(key, val);
        else if( isDataType<double>(type) )
          return set<double>(key, val);
        else if( isDataType<std::string>(type) )
          return set<std::string>(key, val);
        else
          LOG_WARN(name() << ": unknown data type: " << type);

        return false;
      }

      virtual ~Component()
      {
      }

    private:

      template<class Type>
      static inline
      bool setImpl( Component* comp,
                    const std::string& key,
                    const std::string& val );
  };

  template<>
  inline std::string Component::getDataTypeString<bool>()
  {
    return "Bool";
  }
  template<>
  inline std::string Component::getDataTypeString<int>()
  {
    return "Integer";
  }
  template<>
  inline std::string Component::getDataTypeString<double>()
  {
    return "Float";
  }
  template<>
  inline std::string Component::getDataTypeString<std::string>()
  {
    return "String";
  }

  template<>
  inline
  bool Component::setImpl<bool>( Component* comp,
                                 const std::string& name,
                                 const std::string& val )
  {
    return comp->setFlag(name, convertFromString<bool>(val));
  }
  template<>
  inline
  bool Component::setImpl<int>( Component* comp,
                                const std::string& name,
                                const std::string& val )
  {
    return comp->setInteger(name, convertFromString<int>(val));
  }
  template<>
  inline
  bool Component::setImpl<double>( Component* comp,
                                   const std::string& name,
                                   const std::string& val )
  {
    return comp->setFloat(name, convertFromString<double>(val));
  }
  template<>
  inline
  bool Component::setImpl<std::string>( Component* comp,
                                        const std::string& name,
                                        const std::string& val )
  {
    return comp->setString(name, convertFromString<std::string>(val));
  }
}

#endif //LR_COMPONENT
