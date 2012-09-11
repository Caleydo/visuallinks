/*
 * string_utils.h
 *
 *  Created on: 11.09.2012
 *      Author: tom
 */

#ifndef STRING_UTILS_H_
#define STRING_UTILS_H_

#include <sstream>

namespace LinksRouting
{
  namespace
  {
    class Converter
    {
      public:

        template<typename T>
        static
        T convertFromString(const std::string& str, const T& def_val = T())
        {
          T var;
          std::stringstream iss(str, std::ios_base::in);
          iss >> var;

          if( !iss )
            return def_val;

          return var;
        }

        static
        std::string convertFromString( const std::string& str,
                                       const std::string& )
        {
          return str;
        }

        static
        bool convertFromString( const std::string& str,
                                const bool& )
        {
          return str == "true" || str == "1";
        }

      private:
        Converter();
    };
  }

  template<typename T>
  inline
  T convertFromString(const std::string& str, const T& def_val = T())
  {
    return Converter::convertFromString(str, def_val);
  }

  template<typename T>
  std::string convertToString(const T val)
  {
    std::stringstream oss(std::ios_base::out);
    oss << val;

    if( !oss )
      return std::string();

    return oss.str();
  }
} // namespace LinksRouting

#endif /* STRING_UTILS_H_ */
