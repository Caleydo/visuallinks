#ifndef LR_COLOR_INCLUDED
#define LR_COLOR_INCLUDED

namespace LinksRouting
{
//  class Color
//  {
//  public:
//    float rgba[4];
//  };
  struct Color
  {
    float r, g, b, a;

    Color(){};
    Color(float r, float g, float b, float a = 1):
      r(r),
      g(g),
      b(b),
      a(a)
    {}
#if 0
    Color(int r, int g, int b, float a = 1):
      r(r/256.f),
      g(g/256.f),
      b(b/256.f),
      a(a)
    {}
#endif
    operator const float*() const
    {
      return &r;
    }
    Color operator*(float rhs) const
    {
      Color ret;
      ret.r = rhs * r;
      ret.g = rhs * g;
      ret.b = rhs * b;
      ret.a = rhs * a;
      return ret;
    }

    friend inline const Color operator*(float lhs, const Color& rhs)
    {
      return rhs * lhs;
    }
  };

}

#endif
