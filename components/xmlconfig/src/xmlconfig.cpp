#include "xmlconfig.h"

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  XmlConfig::XmlConfig():
    Configurable("XmlConfig"),
    _config(0),
    _dirty_read(false),
    _dirty_write(false)
  {

  }

  //----------------------------------------------------------------------------
  bool XmlConfig::initFrom(const std::string& configstr)
  {
    DocumentPtr doc( new TiXmlDocument(configstr) );
    if( !doc->LoadFile() )
    {
      LOG_ERROR( "Failed to load config from '" << configstr << "': \""
                 << doc->ErrorDesc() << "\" at "
                 << doc->ErrorRow() << ":" << doc->ErrorCol() );
      return false;
    }

    NodePtr config( doc->FirstChild("config") );
    if( !config || !config->ToElement() )
    {
      LOG_ERROR("No 'config' node in document");
      return false;
    }

    _doc.swap(doc);
    _config = config;

    _dirty_read = true;
    _dirty_write = false;

    return true;
  }

  //----------------------------------------------------------------------------
  void XmlConfig::attach(Configurable* component, unsigned int type)
  {
    compInfoList.push_back(CompInfo(component, type));
  }

  bool XmlConfig::startup(Core* core, unsigned int type)
  {
    if(type == Component::Config)
      return true;
    return false;
  }

  void XmlConfig::init()
  {
    //nothing to do
  }

  void XmlConfig::shutdown()
  {
    // ensure everything is written to the config file
    saveFile();
  }

  bool XmlConfig::supports(unsigned int type) const
  {
    return (type & Component::Config);
  }

  //----------------------------------------------------------------------------
  uint32_t XmlConfig::process(unsigned int type)
  {
    if( !_config )
      return 0;

    if( _dirty_write )
    {
      saveFile();
      _dirty_write = false;
    }

    if( _dirty_read )
    {
      loadFile();
      _dirty_read = false;
    }

    return 0;
  }

  //----------------------------------------------------------------------------
  bool XmlConfig::setParameter( const std::string& key,
                                const std::string& val,
                                const std::string& type )
  {
    if( !_config )
      return false;

    std::cout << "setParameter(key = " << key
                         << ", val = " << val
                         << ", type = " << type
                         << ")"
                         << std::endl;

    //resolve parts of identifier (Renderer:FeatureX) and find in config
    std::string valname;
    TiXmlNode* container = parseIdentifier(key, valname, true);

    if( key.length() > valname.length() + 1 )
    {
      std::string component_name =
        key.substr(0, key.length() - valname.length() - 1);
      Configurable* comp = findComponent(component_name);
      if( !comp )
        LOG_ERROR("No such component: " << component_name);
      else
        comp->setParameter(valname, val, type);
    }

    //check if exists
    TiXmlNode* node = container->FirstChild(valname);
    TiXmlElement* arg = 0;
    if( node )
    {
      arg = node->ToElement();

      //check if it matches
      int res = checkTypeMatch(key, arg, type);
      if(res == 1)
        arg->SetAttribute("type", type);
      else if(res == 0)
        return false;

    }
    else
    {
      //create new one
      arg = new TiXmlElement( valname );
      container->LinkEndChild(arg);
      arg->SetAttribute("type", type);
    }

    arg->SetAttribute("val", val);

    _dirty_write = true;

    saveFile(); // TODO check if multiple variables changed within small
                //      time window

    return true;
  }

  //----------------------------------------------------------------------------
  bool XmlConfig::saveFile() const
  {
    if( !_doc )
      return false;
    if( !_dirty_write )
      return true;

    if( !_doc->SaveFile() )
    {
      LOG_ERROR("Failed to save config to '" << _doc->ValueStr() << "'");
      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  bool XmlConfig::loadFile()
  {
    if( !_config )
      return false;
    if( !_dirty_read )
      return true;

    // For now just process xml config once. We could check for file
    // modifications every second and update config if a change occurs...

    // Go through all the container, find all matching components and set
    // parameters
    for( TiXmlNode* child = _config->FirstChild();
         child;
         child = child->NextSibling() )
    {
      if( child->Type() != TiXmlNode::TINYXML_ELEMENT )
        continue;

      const std::string& comp_name = child->ValueStr();
      Type type = StringToType(comp_name);

      bool match = false;

      for( CompInfoList::iterator it = compInfoList.begin();
           it != compInfoList.end();
           ++it )
      {
        if( !it->match(comp_name, type) )
          continue;

        match = true;

        for( TiXmlNode* arg = child->FirstChild();
             arg;
             arg = arg->NextSibling() )
        {
          TiXmlElement* argel = arg->ToElement();
          if( !argel )
            continue;

          const std::string& key = argel->ValueStr();
          const char* argtype = argel->Attribute("type");
          const char* valstr = argel->Attribute("val");
          if(!argtype || !valstr)
          {
            LOG_WARN("incomplete Argument: "  << comp_name << ":" << key);
            continue;
          }

          if( !it->comp->setParameter(argel->ValueStr(), valstr, argtype) )
            LOG_WARN( "Failed to set value '"
                      << argel->ValueStr() << "' of type '"
                      << argtype << "' to '"
                      << valstr << "' on component "
                      << it->comp->name() );
        }
      }

      if( !match )
        LOG_WARN("Failed to find matching component for '" << comp_name << "'");
    }

    return true;
  }

  int XmlConfig::checkTypeMatch( const std::string& identifier,
                                 TiXmlElement* arg,
                                 const std::string& type )
  {
    const std::string * typestr;
    if(!(typestr = arg->Attribute(std::string("type"))))
    {
      std::cout << "LinksSystem XmlConfig Warning: no type specified for \"" << identifier << "\"" << std::endl;
      return 1;
    }
    else if(typestr->compare(type) != 0)
    {
      std::cout << "LinksSystem XmlConfig Warning: types do not match for \"" << identifier << "\":" << std::endl;
      std::cout << "  " << *typestr << " != " << type << std::endl;
      return 0;
    }
    return 2;
  }

  //----------------------------------------------------------------------------
  XmlConfig::NodePtr XmlConfig::parseIdentifier( const std::string& identifier,
                                                 size_t pos,
                                                 NodePtr container,
                                                 std::string& valname,
                                                 bool create )
  {
    size_t p = identifier.find_first_of(':', pos);
    if(p == std::string::npos)
    {
      valname = identifier.substr(pos);
      return container;
    }

    assert(container);

    std::string containername = identifier.substr(pos, p-pos);
    TiXmlElement* child = 0;
    TiXmlNode* node = container->FirstChild(containername);
    if( !node )
    {
      if( !create )
        return 0;

      child = new TiXmlElement( containername );
      container->LinkEndChild(child);
    }
    else
      child = node->ToElement();

    return parseIdentifier(identifier, p+1, child, valname, create);
  }

  //----------------------------------------------------------------------------
  XmlConfig::NodePtr XmlConfig::parseIdentifier( const std::string& identifier,
                                                 std::string& valname,
                                                 bool create )
  {
    return parseIdentifier(identifier, 0, _config, valname, create);
  }

  //----------------------------------------------------------------------------
  Configurable* XmlConfig::findComponent(const std::string& identifier)
  {
    Type type = StringToType(identifier);

    for( CompInfoList::iterator it = compInfoList.begin();
         it != compInfoList.end();
         ++it )
    {
      if( it->match(identifier, type) )
        return it->comp;
    }

    return 0;
  }
}
