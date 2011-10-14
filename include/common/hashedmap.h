
#ifndef LR_HASHEDMAP
#define LR_HASHEDMAP
#include <component.h>
#include <map>
#include <string>

namespace LinksRouting
{
  class hashedentry
  {
    unsigned long hash(const std::string& s) const 
    {
        unsigned long hashv = 5381;
        for(size_t i = 0; i < s.length(); ++i)
	        hashv = ((hashv << 5) + hashv) + static_cast<unsigned long>(s[i]); // hash * 33 + character
        return hashv;
    }
    const std::string _str;
    const unsigned long _hash;
    public:
      hashedentry(const std::string& str) : _str(str), _hash(hash(str)) { }
      bool operator < (const hashedentry& other) const 
      { 
        if(_hash != other._hash)
	  return _hash < other._hash;
        return _str < other._str;
      }
      const std::string & str() const { return _str; }
      operator std::string () const { return _str; }
  };
  template<class Value, class Compare = std::less<hashedentry>,  class Allocator = std::allocator<std::pair<hashedentry,Value> > >
  struct HashedMap
  {
    typedef std::map<hashedentry, Value, Compare, Allocator> Type;
  };

};
#endif //LR_HASHEDMAP