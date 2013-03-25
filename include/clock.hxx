/*
 * clock.hxx
 *
 *  Created on: 15.01.2013
 *      Author: tom
 */

#ifndef CLOCK_HXX_
#define CLOCK_HXX_

#include <chrono>
#include <iosfwd>

template <typename V, typename R>
std::ostream& operator << (std::ostream& s, const std::chrono::duration<V,R>& d)
{
  auto dur_us =
    std::chrono::duration_cast<std::chrono::microseconds>(d).count();

  if( dur_us < 1000 )
    s << dur_us << "us";
  else
    s << (dur_us / 1000.f) << "ms";
  return s;
}

namespace LinksRouting
{
  typedef std::chrono::high_resolution_clock clock;
} // namespace LinksRouting

#if 0
#define PROFILE_START() \
  LinksRouting::clock::time_point profile_start = LinksRouting::clock::now();

#define PROFILE_LAP(name) \
  std::cout << "profile " << name << " " \
            << (LinksRouting::clock::now() - profile_start) \
            << std::endl;

#define PROFILE_RESULT(name) \
  PROFILE_LAP(name) \
  profile_start = LinksRouting::clock::now();
#else
# define PROFILE_START()
# define PROFILE_LAP(name)
# define PROFILE_RESULT(name)
#endif
#endif /* CLOCK_HXX_ */
