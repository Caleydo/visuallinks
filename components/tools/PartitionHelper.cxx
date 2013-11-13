/*
 * PartitionHelper.cxx
 *
 *  Created on: 25.11.2012
 *      Author: tom
 */

#include "PartitionHelper.hxx"
#include <algorithm>

//------------------------------------------------------------------------------
void PartitionHelper::add(const float2& interval)
{
  // Keep intervals always ordered and merge if overlapping
  for( auto part = _partitions.begin();
            part != _partitions.end();
          ++part )
  {
    // Completely before current interval
    if( interval.y < part->x )
    {
      _partitions.insert(part, interval);
      return;
    }

    // Completely after current interval
    if( interval.x > part->y )
      continue;

    // Merge
    *part = float2( std::min(part->x, interval.x),
                    std::max(part->y, interval.y) );

    // Merge all following, overlapping intervals
    auto part2 = part;
    for( ++part2;
           part2 != _partitions.end();
           part2 = _partitions.erase(part2) )
    {
      if( part2->x > part->y )
        break;

      part->y = std::max(part->y, part2->y);
    }

    return;
  }

  _partitions.push_back(interval);
}

//------------------------------------------------------------------------------
void PartitionHelper::clip(float min, float max, float cut)
{
  Partitions::iterator part = _partitions.begin();
  if( part == _partitions.end() )
    return;

  if( part->x <= 0 )
    part->x = 0;
  else
    part->x += cut;
  part->y -= cut;

  if( part->y <= part->x )
    part = _partitions.erase(part);
  if( part == _partitions.end() )
    return;

  for(++part; part != _partitions.end(); ++part)
  {
    part->x += cut;

    if( part->y >= max )
      part->y = max;
    else
      part->y -= cut;

    if( part->y <= part->x )
      part = _partitions.erase(part);
  }
}

//------------------------------------------------------------------------------
void PartitionHelper::print()
{
  for(auto part = _partitions.begin();
           part != _partitions.end();
         ++part)
    std::cout << "[" << part->x << ", " << part->y << "] ";
  std::cout << std::endl;
}

//------------------------------------------------------------------------------
const Partitions& PartitionHelper::getPartitions() const
{
  return _partitions;
}

#if 0
//------------------------------------------------------------------------------
Partitions PartitionHelper::getInverted(float min, float max)
{
  Partitions partitions;
  auto part = _partitions.begin();
  if( part == _partitions.end() )
    return partitions;

  if( part->x > min )
    partitions.push_back( float2(min, part->x) );

  auto prev = part;
  ++part;

  for(; part != _partitions.end(); ++prev, ++part)
    partitions.push_back( float2(prev->y, part->x) );

  --part;
  if( part->y < max )
    partitions.push_back( float2(part->y, max) );

  return partitions;
}
#endif

//------------------------------------------------------------------------------
std::string to_string(const Partitions& p)
{
  std::stringstream strm;
  strm << '[';
  for(auto part = p.begin(); part != p.end(); ++part)
  {
    if( part != p.begin() )
      strm << ',';
    strm << '[' << static_cast<int>(part->x + 0.5)
         << ',' << static_cast<int>(part->y + 0.5)
         << ']';
  }
  strm << ']';
  return strm.str();
}
