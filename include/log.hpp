/*!
 * @file log.hpp
 * @brief
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 02.03.2012
 */

#ifndef _LOG_HPP_
#define _LOG_HPP_

#include <boost/current_function.hpp>
#include <iostream>
#include <sstream>

#define LOG_MSG(strm, type, msg)\
  strm << "[" << type << "] '" << msg << "' in " << BOOST_CURRENT_FUNCTION << std::endl

#define LOG_DEBUG(msg) //LOG_MSG(std::cout,  "DEBUG", msg)
#define LOG_INFO(msg) LOG_MSG(std::cout,  "INFO", msg)
#define LOG_WARN(msg) LOG_MSG(std::cout,  "WARNING", msg)
#if 0
  {\
    LOG_MSG(std::cout,  "WARNING", msg);\
    std::ostringstream str;\
    str << "/usr/bin/notify-send Warning \"" << msg << '"';\
    system(str.str().c_str());\
  }
#endif
#define LOG_ERROR(msg) LOG_MSG(std::cerr,  "ERROR", msg)
#endif /* _LOG_HPP_ */
