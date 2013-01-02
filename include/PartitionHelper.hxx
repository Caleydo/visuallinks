#ifndef PARTITION_HELPER_HXX_
#define PARTITION_HELPER_HXX_

#include <list>
#include "float2.hpp"
#include <iostream>

typedef std::list<float2> Partitions;
class PartitionHelper
{
  public:
    void add(const float2& interval);
    void clip(float min, float max, float cut = 0);
    void print();
    const Partitions& getPartitions() const;

  protected:
    Partitions _partitions;
};

std::string to_string(const Partitions& p);

#endif /* PARTITION_HELPER_HXX_ */
