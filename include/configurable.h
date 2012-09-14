/*
 * configurable.h
 *
 *  Created on: 14.09.2012
 *      Author: tom
 */

#ifndef CONFIGURABLE_H_
#define CONFIGURABLE_H_

#include "log.hpp"
#include "string_utils.h"
#include <string>

namespace LinksRouting
{
  class Configurable
  {
    public:

      const std::string& name() const { return _name; }

      template<class Type>
      inline static std::string getDataTypeString();

      template<class Type>
      inline static bool isDataType(const std::string& type)
      {
        return getDataTypeString<Type>() == type;
      }

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

      virtual ~Configurable() {}

    protected:

      Configurable(const std::string& name):
        _name( name )
      {}

    private:

      std::string _name;

      template<class Type>
      static inline
      bool setImpl( Configurable* comp,
                    const std::string& key,
                    const std::string& val );
  };

  template<>
  inline std::string Configurable::getDataTypeString<bool>()
  {
    return "Bool";
  }
  template<>
  inline std::string Configurable::getDataTypeString<int>()
  {
    return "Integer";
  }
  template<>
  inline std::string Configurable::getDataTypeString<double>()
  {
    return "Float";
  }
  template<>
  inline std::string Configurable::getDataTypeString<std::string>()
  {
    return "String";
  }

  template<>
  inline
  bool Configurable::setImpl<bool>( Configurable* comp,
                                    const std::string& name,
                                    const std::string& val )
  {
    return comp->setFlag(name, convertFromString<bool>(val));
  }
  template<>
  inline
  bool Configurable::setImpl<int>( Configurable* comp,
                                   const std::string& name,
                                   const std::string& val )
  {
    return comp->setInteger(name, convertFromString<int>(val));
  }
  template<>
  inline
  bool Configurable::setImpl<double>( Configurable* comp,
                                      const std::string& name,
                                      const std::string& val )
  {
    return comp->setFloat(name, convertFromString<double>(val));
  }
  template<>
  inline
  bool Configurable::setImpl<std::string>( Configurable* comp,
                                           const std::string& name,
                                           const std::string& val )
  {
    return comp->setString(name, convertFromString<std::string>(val));
  }

} // namespace LinksRouting

#endif /* CONFIGURABLE_H_ */
