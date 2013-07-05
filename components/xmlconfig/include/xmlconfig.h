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
    static int checkTypeMatch( const std::string& identifier,
                               TiXmlElement* arg,
                               const std::string& type );

    template<class T>
    static int checkTypeMatch(const std::string& identifier, TiXmlElement* arg)
    {
        return checkTypeMatch(identifier, arg, getDataTypeString<T>());
    }


    template<class Type>
    bool setParameterHelper(const std::string& identifier, const Type val)
    {
      std::stringstream argstr;
      argstr.precision(64);
      argstr << val;

      return setParameter(identifier, argstr.str(), getDataTypeString<Type>());
    }

    template<class T>
    bool getParameter(const std::string& identifier, T& val) const
    {
      size_t end = identifier.find_first_of(':');
      const std::string& comp = identifier.substr(0, end);
      const std::string& id = end != std::string::npos
                            ? identifier.substr(end + 1)
                            : "";
      if( id.empty() )
        return false;

      for( CompInfoList::const_iterator it = compInfoList.begin();
           it != compInfoList.end();
           ++it )
      {
        if( it->comp->name() == comp )
          return it->comp->get(id, val);
      }

      if( !_config )
        return false;

      //resolve parts of identifier (Renderer:FeatureX) and find in config
      std::string valname;
      TiXmlNode* container = const_cast<XmlConfig*>(this)->parseIdentifier(identifier, valname, false);
      if( !container )
        return false;

      //check if exists
      TiXmlNode* arg_node = container->FirstChild(valname);
      if( !arg_node )
        return false;
      TiXmlElement* arg = arg_node->ToElement();

      //check if types match
      if(checkTypeMatch<T>(identifier, arg) < 2)
        return false;

      const char* valstr = arg->Attribute("val");
      if( !valstr )
        return false;

      std::stringstream argvalstr(valstr);
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

      bool match(const std::string& name, Type type)
      {
        if(type != None && (is & type) != 0)
          //type based
          return true;
        else if( comp->name() == name )
          //string based
          return true;

        return false;
      }
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
    /**
     * @param identifier    Type or name of component
     */
    Configurable* findComponent(const std::string& identifier);

  public:
    XmlConfig();

    virtual bool initFrom(const std::string& config);
    virtual void attach(Configurable* component, unsigned int type);

    bool startup(Core* core, unsigned int type);
    void init();
    void shutdown();
    bool supports(unsigned int type) const;

    uint32_t process(unsigned int type) override;

    virtual bool setParameter( const std::string& key,
                               const std::string& val,
                               const std::string& type );

    bool setFlag(const std::string& name, bool val)
    {
      return setParameterHelper(name, val);
    }
    bool getFlag(const std::string& name, bool& val) const
    {
      return getParameter(name, val);
    }
    bool setInteger(const std::string& name, int val)
    {
      return setParameterHelper(name, val);
    }
    bool getInteger(const std::string& name, int& val) const
    {
      return getParameter(name, val);
    }
    bool setFloat(const std::string& name, double val)
    {
      return setParameterHelper(name, val);
    }
    bool getFloat(const std::string& name, double& val) const
    {
      return getParameter(name, val);
    }
    bool setString(const std::string& name, const std::string& val)
    {
      return setParameterHelper(name, val);
    }
    bool getString(const std::string& name, std::string& val) const
    {
      return getParameter(name, val);
    }
  };

}

#endif /* LR_XMLCONFIG */
