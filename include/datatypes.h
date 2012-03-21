#ifndef DATATYPES_H
#define DATATYPES_H


#ifdef _WIN32
  typedef unsigned int uint32_t;
#endif


struct uint4
{
  union
  {
    unsigned int v[4];
    struct
    {
      unsigned int x,y,z,w;
    };
  };
  uint4(unsigned int _x, unsigned int _y, unsigned int _z, unsigned int _w) : x(_x), y(_y), z(_z), w(_w) { }
};

struct int4
{
  union
  {
    int v[4];
    struct
    {
      int x,y,z,w;
    };
  };
  int4( int _x, int _y, int _z, int _w) : x(_x), y(_y), z(_z), w(_w) { }
  bool operator < (const int4& other) const
  {
    if(x < other.x) return true;
    else if(x == other.x)
    {
      if(z < other.z) return true;
      else if(z == other.z)
      {
        if(y < other.y) return true;
        else if(y == other.y && w < other.w)
          return true;
      }
    }
    return false;
  }
};
template<class T>
struct int4Aug
{
  T aug;
  union
  {
    int v[4];
    struct
    {
      int x,y,z,w;
    };
  };
  int4Aug( int _x, int _y, int _z, int _w, T _aug = 0) : x(_x), y(_y), z(_z), w(_w), aug(_aug) { }
  bool operator < (const int4Aug& other) const
  {
    if(x < other.x) return true;
    else if(x == other.x)
    {
      if(z < other.z) return true;
      else if(z == other.z)
      {
        if(y < other.y) return true;
        else if(y == other.y && w < other.w)
          return true;
      }
    }
    return false;
  }
  operator int4()
  {
    return int4(x,y,z,w);
  }
};

#endif //DATATYPES_H