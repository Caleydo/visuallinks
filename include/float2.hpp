/*!
 * @file float2.hpp
 * @brief Provides a simple 2d vector
 * @details
 * @author Thomas Geymayer <tomgey@gmail.com>
 * @date Date of Creation: 12.10.2011
 */

#ifndef _FLOAT2_HPP_
#define _FLOAT2_HPP_

#include <cmath>
#include <iostream>

/**
 * A simple 2d vector
 */
struct float2
{
  float x;
  float y;

  /**
   *
   */
  float2(): x(), y() {}

  /**
   *
   */
  float2(float _x, float _y) : x(_x), y(_y) { }

  /**
   *
   */
  float length() const
  {
    return std::sqrt(x*x + y*y);
  }

  /**
   *
   */
  float2& normalize()
  {
    float l = length();
    x /= l;
    y /= l;

    return *this;
  }

  /**
   *
   */
  float dot(const float2& rhs) const
  {
    return x * rhs.x + y * rhs.y;
  }
};

/**
 *
 */
inline float2 operator +(const float2& a, const float2& b)
{
  return float2(a.x + b.x, a.y + b.y);
}

/**
 *
 */
inline float2 operator -(const float2& a, const float2& b)
{
  return float2(a.x - b.x, a.y - b.y);
}

/**
 *
 */
inline float2 operator -(const float2& a)
{
  return float2(-a.x, -a.y);
}

/**
 *
 */
inline const float2 operator *(float a, const float2& v)
{
  return float2(a * v.x, a * v.y);
}

/**
 *
 */
inline const float2 operator *(const float2& v, float a)
{
  return a * v;
}

/**
 *
 */
inline const float2 operator /(const float2& v, float a)
{
  return float2(v.x / a, v.y / a);
}

/**
 *
 */
inline std::ostream& operator<<(std::ostream& strm, const float2& p)
{
  return strm << "(" << p.x << "|" << p.y << ")";
}

#endif /* _FLOAT2_HPP_ */
