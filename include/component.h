#ifndef LR_COMPONENT
#define LR_COMPONENT
#include <string>

namespace LinksRouting
{
  class Core;
  class Component
  {
  public:
    enum Type
    {
      None = 0,
      Config = 1,
      Proxy = 2,
      TransparencyAnalysis = 4,
      Costanalysis = 8,
      Routing = 16,
      Renderer = 32,      
      Any = 63
    };
    static std::string TypeToString(Type t)
    {
      switch(t)
      {
      case Config:
        return "ComponentConfig";
      case Proxy:
        return "ComponentProxy";
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
      if(str.compare("ComponentConfig") == 0)
        return Config;
      else if(str.compare("ComponentProxy") == 0)
        return Proxy;
      else if(str.compare("ComponentTransparencyAnalysis") == 0)
        return TransparencyAnalysis;
      else if(str.compare("ComponentCostanalysis") == 0)
        return Costanalysis;
      else if(str.compare("ComponentRenderer") == 0)
        return Renderer;
      else if(str.compare("ComponentRouting") == 0)
        return Routing;
      else if(str.compare("ComponentAny") == 0)
        return Any;
      return None;
    }
    struct MapData
    {
      enum MapDatatype
      {
        ImageRGBA8,
        ImageGray8,
        ImageRGBA32F,
        ImageGray32F,
        OpenGLTexture,
        OpenCLTexture,
        OpenCLBufferRGBA8,
        OpenCLBufferGray8,
        OpenCLBufferRGBA32F,
        OpenCLBufferGray32F
      };
      MapDatatype type;
      union {
        unsigned int id;
        unsigned char* pdata;
      };
      unsigned int width, height;
      unsigned int layers;
    };

    virtual bool startup(Core* core, unsigned int type) = 0;
    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual bool supports(Type type) const = 0;
    virtual const std::string& name() const = 0;

    virtual void process(Type type) = 0;

    virtual bool setFlag(const std::string& name, bool val) = 0;
    virtual bool getFlag(const std::string& name, bool& val) const = 0;
    virtual bool setInteger(const std::string& name, int val) = 0;
    virtual bool getInteger(const std::string& name, int& val) const = 0;
    virtual bool setFloat(const std::string& name, double val) = 0;
    virtual bool getFloat(const std::string& name, double& val) const = 0;
    virtual bool setString(const std::string& name, const std::string& val) = 0;
    virtual bool getString(const std::string& name, std::string& val) const = 0;

    virtual ~Component() {}
  };
};

#endif //LR_COMPONENT