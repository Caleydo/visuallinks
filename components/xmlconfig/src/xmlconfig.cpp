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
  void XmlConfig::process(unsigned int type)
  {
    if( !_config )
      return;

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
      const std::string& comp_name = child->ValueStr();
      Type type = StringToType(comp_name);

      for( CompInfoList::iterator it = compInfoList.begin();
           it != compInfoList.end();
           ++it )
      {
        Configurable* comp = 0;
        if(type != None && (it->is & type) != 0)
          //type based
          comp = it->comp;
        else if( it->comp->name() == comp_name )
          //string based
          comp = it->comp;

        if( !comp )
          continue;

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

          if( !comp->set(argel->ValueStr(), valstr, argtype) )
            LOG_WARN( "Failed to set value of type '"
                      << argtype << "' to '"
                      << valstr << "' on component "
                      << comp->name() );
        }
      }
    }

    return true;
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

  //----------------------------------------------------------------------------
  XmlConfig::NodePtr XmlConfig::parseIdentifier( const std::string& identifier,
                                                 std::string& valname,
                                                 bool create )
  {
    return parseIdentifier(identifier, 0, _config, valname, create);
  }
}
