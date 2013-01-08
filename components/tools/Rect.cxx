/*
 * Rect.cxx
 *
 *  Created on: 21.11.2012
 *      Author: tom
 */

#include "float2.hpp"

//------------------------------------------------------------------------------
Rect::Rect():
  pos(0,0),
  size(-1,-1)
{

}

//------------------------------------------------------------------------------
Rect::Rect(const float2& pos, const float2& size):
  pos(pos),
  size(size)
{

}

//------------------------------------------------------------------------------
void Rect::expand(const float2& p)
{
  if( size.x < 0 || size.y < 0 )
  {
    pos = p;
    size = float2(0,0);
  }
  else
  {
    float left   = std::min(p.x, l()),
          right  = std::max(p.x, r()),
          top    = std::min(p.y, t()),
          bottom = std::max(p.y, b());

    pos.x = left;
    pos.y = top;
    size.x = right - left;
    size.y = bottom - top;
  }
}

//------------------------------------------------------------------------------
bool Rect::contains(float x, float y, float margin) const
{
  return x >= pos.x - margin && x <= pos.x + size.x + margin
      && y >= pos.y - margin && y <= pos.y + size.y + margin;
}

//------------------------------------------------------------------------------
std::string Rect::toString(bool round) const
{
  std::stringstream strm;
  auto do_round =  [round](float num)->float{ if(round) return static_cast<float>(std::floor(num + 0.5)); else return num; };

  strm << "{\"x\":" << do_round(pos.x) << ","
           "\"y\":" << do_round(pos.y) << ","
           "\"width\":" << do_round(size.x) << ","
           "\"height\":" << do_round(size.y) << "}";
  return strm.str();
}

//------------------------------------------------------------------------------
Rect& Rect::operator *=(float a)
{
  pos *= a;
  size *= a;
  return *this;
}
