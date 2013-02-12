/*
 * qt_helper.cxx
 *
 *  Created on: 12.02.2013
 *      Author: tom
 */

#include "qt_helper.hxx"

//------------------------------------------------------------------------------
std::string to_string(const QString& str)
{
  const QByteArray& bytes =
#ifdef _WIN32
      str.toLocal8Bit();
#else
      str.toUtf8();
#endif
  return std::string(bytes.data(), bytes.size());
}

//------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& strm, const QString& str)
{
  return strm << to_string(str);
}
