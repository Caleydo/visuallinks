#include "xmlconfig.h"

namespace LinksRouting
{
  XmlConfig::XmlConfig() : isInit(false), myname("XmlConfig")
  {

  }

  bool XmlConfig::initFrom(const std::string& configstr)
  {
    doc = new TiXmlDocument(configstr);
    if(!doc->LoadFile())
    {
      std::cerr << "LinksSystem XmlConfig Error, can not load config " << configstr << ": \"" <<  doc->ErrorDesc() << "\"" << std::endl << " at " << doc->ErrorRow() << ":" << doc->ErrorCol() << std::endl;
      return false;
    }

    config = doc->FirstChild("config");
    if(config == NULL || config->ToElement() == NULL)
    {
      std::cerr << "LinksSystem XmlConfig Error: no \"config\" node in document" << std::endl;
      return false;
    }
    isInit = true;
    return true;
  }
  void XmlConfig::attach(Component* component, unsigned int type)
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
    if(isInit)
      delete config;
  }
  bool XmlConfig::supports(Type type) const
  {
    return type == Component::Config;
  }

  void XmlConfig::process(Type type)
  {
    if(!isInit)
      return;

    //for now just process xml config once
    //we could check for file modifications every second and update config if a change occurs...
    static bool processed = false;
    if( processed )
      return;
    processed = true;

    //go through all the container, find all matching components and set parameters
    for ( TiXmlNode* child = config->FirstChild(); child; child = child->NextSibling())
    {
      Type type = StringToType(child->ValueStr());
      for(CompInfoList::iterator it = compInfoList.begin(); it != compInfoList.end(); ++it)
      {
        Component* comp = 0;
		    if(type != None && (it->is & type) != 0)
          //type based
          comp = it->comp;
        else if(it->comp->name().compare(child->ValueStr()) == 0)
          //string based
          comp = it->comp;

        if( !comp )
          continue;

        for(TiXmlNode* arg = child->FirstChild(); arg; arg = arg->NextSibling() )
        {
          TiXmlElement* argel = arg->ToElement();
          if( !argel )
            continue;

          std::string valname = argel->ValueStr();
          const char* argtype = argel->Attribute("type");
          const char* valstr = argel->Attribute("val");
          if(!argtype || !valstr)
          {
            std::cout << "LinksSystem XmlConfig Warning: incomplete Argument: "  << child->ValueStr() << ":" << valname << std::endl;
            continue;
          }
          std::stringstream valstrstr(valstr);

          if(getTypeString<bool>().compare(argtype) == 0)
          {
            bool val;
            valstrstr >> std::boolalpha >> val;
            if(!comp->setFlag(valname, val))
              std::cout << "LinksSystem XmlConfig Warning: could not set Bool Argument "  << child->ValueStr() << ":" << valname << " for Component " << comp->name() << std::endl;
          }
          else if(getTypeString<int>().compare(argtype) == 0)
          {
            int val;
            valstrstr >> val;
            if(!comp->setInteger(valname, val))
              std::cout << "LinksSystem XmlConfig Warning: could not set Int Argument "  << child->ValueStr() << ":" << valname << " for Component " << comp->name() << std::endl;
          }
          else if(getTypeString<double>().compare(argtype) == 0)
          {
            double val;
            valstrstr >> val;
            if(!comp->setFloat(valname, val))
              std::cout << "LinksSystem XmlConfig Warning: could not set Double Argument "  << child->ValueStr() << ":" << valname << " for Component " << comp->name() << std::endl;
          }
          else if(getTypeString<std::string>().compare(argtype) == 0)
          {
            std::string val(valstr);
            if(!comp->setString(valname, val))
              std::cout << "LinksSystem XmlConfig Warning: could not set Double Argument "  << child->ValueStr() << ":" << valname << " for Component " << comp->name() << std::endl;
          }
          else
            std::cout << "LinksSystem XmlConfig Warning: unknown type \"" << argtype << "\" of argument " << child->ValueStr() << ":" << valname << std::endl;
        }
      }
    }
  }
}
