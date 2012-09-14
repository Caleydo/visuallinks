#ifndef LR_XMLCONFIG
#define LR_XMLCONFIG

#include <list>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <config.h>
#ifndef TIXML_USE_STL
#define TIXML_USE_STL
#endif
#include <tinyxml.h>

namespace LinksRouting
{
  class XmlConfig:
    public Config
  {
    template<class Type>
    static int checkTypeMatch(const std::string& identifier, TiXmlElement* arg)
    {
        const std::string * typestr;
        if(!(typestr = arg->Attribute(std::string("type"))))
        {
          std::cout << "LinksSystem XmlConfig Warning: no type specified for \"" << identifier << "\"" << std::endl;
          return 1;
        }
        else if(typestr->compare(getDataTypeString<Type>()) != 0)
        {
          std::cout << "LinksSystem XmlConfig Warning: types do not match for \"" << identifier << "\":" << std::endl;
          std::cout << "  " << typestr << " != " << getDataTypeString<Type>() << std::endl;
          return 0;
        }
        return 2;
    }

    template<class Type>
    bool setParameter(const std::string& identifier, Type val)
    {
      if( !_config )
        return false;

      //resolve parts of identifier (Renderer:FeatureX) and find in config
      std::string valname;
      TiXmlNode* container = parseIdentifier(identifier, valname, true);
      
      //check if exists
      TiXmlElement* arg = container->FirstChild(valname)->ToElement();
      std::stringstream argstr;
      argstr.precision(64);
      argstr << val;
      if(arg)
      {
        //check if it matches
        int res = checkTypeMatch<Type>(identifier, arg);
        if(res == 1)
          arg->SetAttribute("type", getDataTypeString<Type>());
        else if(res == 0)
          return false;
        arg->SetAttribute("val", argstr.str());
      }
      else
      {
        //create new one
        arg = new TiXmlElement( valname );
        container->LinkEndChild(arg);
        arg->SetAttribute("type", getDataTypeString<Type>());
        arg->SetAttribute("val", argstr.str());
      }
      _dirty_write = true;
      return true;
    }
    template<class Type>
    bool getParameter(const std::string& identifier, Type& val) const
    {
      if( !_config )
        return false;

      //resolve parts of identifier (Renderer:FeatureX) and find in config
      std::string valname;
      TiXmlNode* container = const_cast<XmlConfig*>(this)->parseIdentifier(identifier, valname, false);
      
      //check if exists
      TiXmlElement* arg = container->FirstChild(valname)->ToElement();
      if(!arg)
        return false;
      //check if types match
      if(checkTypeMatch<Type>(identifier, arg) < 2)
        return false;
      const std::string* valstr = arg->Attribute(valname);
      std::stringstream argvalstr(*valstr);
      argvalstr >> val;
      return true;
    }


    struct CompInfo
    {
      Configurable* comp;
      unsigned int is;
      CompInfo(Configurable* c, unsigned int types):
        comp(c),
        is(types)
      {}
    };
    typedef std::list<CompInfo> CompInfoList;

    typedef std::unique_ptr<TiXmlDocument> DocumentPtr;
    typedef TiXmlNode* NodePtr;

    DocumentPtr _doc;
    NodePtr     _config;

    bool _dirty_read,
         _dirty_write;

    CompInfoList compInfoList;
    
    bool saveFile() const;
    bool loadFile();

    NodePtr parseIdentifier( const std::string& identifier,
                             size_t pos,
                             NodePtr container,
                             std::string& valname,
                             bool create );
    NodePtr parseIdentifier( const std::string& identifier,
                             std::string& valname,
                             bool create = false );

  public:
    XmlConfig();

    virtual bool initFrom(const std::string& config);
    virtual void attach(Configurable* component, unsigned int type);

    bool startup(Core* core, unsigned int type);
    void init();
    void shutdown();
    bool supports(unsigned int type) const;

    void process(unsigned int type);

    bool setFlag(const std::string& name, bool val)
    {
      return setParameter(name, val);
    }
    bool getFlag(const std::string& name, bool& val) const
    {
      return getParameter(name, val);
    }
    bool setInteger(const std::string& name, int val)
    {
      return setParameter(name, val);
    }
    bool getInteger(const std::string& name, int& val) const
    {
      return getParameter(name, val);
    }
    bool setFloat(const std::string& name, double val)
    {
      return setParameter(name, val);
    }
    bool getFloat(const std::string& name, double& val) const
    {
      return getParameter(name, val);
    }
    bool setString(const std::string& name, const std::string& val)
    {
      return setParameter(name, val);
    }
    bool getString(const std::string& name, std::string& val) const
    {
      return getParameter(name, val);
    }
  };

}

#endif /* LR_XMLCONFIG */
