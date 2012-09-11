#ifndef LR_XMLCONFIG
#define LR_XMLCONFIG

#include <list>
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
  class XmlConfig : public Config
  {
    TiXmlNode* parseIdentifier(const std::string& identifier, size_t pos, TiXmlNode* container, std::string& valname, bool create)
    {
      size_t p = identifier.find_first_of(':', pos);
      if(p == std::string::npos)
      {
        valname = identifier.substr(pos);
        return container;
      }
      std::string containername = identifier.substr(pos, p-pos);
      TiXmlElement * child = container->FirstChild(containername)->ToElement();
      if(!child && !create)
        return 0;
      else if(!child)
      {
        child = new TiXmlElement( containername );
        container->LinkEndChild(child);
      }
      return parseIdentifier(identifier, p+1, child, valname, create);
    }
    TiXmlNode* parseIdentifier(const std::string& identifier, std::string& valname, bool create = false)
    {
      return parseIdentifier(identifier, 0, config, valname, create);
    }

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
      if(!isInit)
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
      return true;
    }
    template<class Type>
    bool getParameter(const std::string& identifier, Type& val) const
    {
      if(!isInit)
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
      Component* comp;
      unsigned int is;
      CompInfo(Component* c, unsigned int types) : comp(c), is(types) { }
    };
    typedef std::list<CompInfo> CompInfoList;

    bool isInit;
    TiXmlDocument *doc;
    TiXmlNode *config;
    CompInfoList compInfoList;
    const std::string myname;
    
  public:
    XmlConfig();

    bool initFrom(const std::string& config);
    void attach(Component* component, unsigned int type);

    bool startup(Core* core, unsigned int type);
    void init();
    void shutdown();
    bool supports(unsigned int type) const;
    const std::string& name() const
    {
      return myname;
    }

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

#endif
