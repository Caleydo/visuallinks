
float getPenalty(read_only image2d_t costmap, int2 pos)
{
  const sampler_t sampler = CLK_FILTER_NEAREST
                          | CLK_NORMALIZED_COORDS_FALSE
                          | CLK_ADDRESS_CLAMP_TO_EDGE;

  return read_imagef(costmap, sampler, pos).x;
}

__kernel void prepareRouting(read_only image2d_t costmap,
                           global uint* nodes,
                           const int2 dim)
{
  int2 id = {get_global_id(0), get_global_id(1)};
  nodes[dim.x*id.y+id.x] = 255*min(1.0f,getPenalty(costmap, id));
}

#if 0

/*
 The values stored in the resulting map contain the cost the reach each node and
 which is its predecessor on the path there. It contains 32 Bits for each node
 where the first two bits represent the direction of the predecessor and the
 remaining bits form the actual cost of reaching the node. 
 */

constant uint MASK_VALUE        = 0x3fffffff;
constant uint MASK_DIRECTION    = 0xc0000000;
constant uint VALUE_NOT_VISITED = 0xffffffff;

//------------------------------------------------------------------------------
// Heap
//------------------------------------------------------------------------------
typedef struct
{
  global uint *values; //!< The values to be sorted
  global uint *heap;   //!< Sorted indices (by values)
  uint size;           //!< Current heap size
  int2 dim;            //!< Dimensions of values
} data_t;

#define _value(index) (d->values[ d->heap[index] ] & MASK_VALUE)
#define _parent(index) (index / 2)
#define _child_left(index) (2 * index + 1)
#define _child_right(index) (2 * index + 2)

/**
 * @param index The index of the node to insert
 */
void heap_insert( data_t *d,
                  const uint index )
{
  uint cur = d->size++;
  uint val = _value(cur);
  uint parent = _parent(cur);

  // move up
  while( cur && val < _value(parent) )
  {
    d->heap[ cur ] = d->heap[ parent ];

    cur = parent;
    parent = _parent(cur);
  }
  
  d->heap[ cur ] = index;
}

/**
 *
 * @return Index of the minimum node
 */
uint heap_removeMin( data_t *d )
{
  // Remove first element
  uint cur = 0,
       front = d->heap[ cur ],
       child = _child_left(cur);
  
  // Propagate hole down the tree
  while( child < d->size )
  {
    // Change to right child if it exists and is smaller than the left one
    if( child + 1 < d->size && _value(child + 1) < _value(child) )
      child += 1;

    d->heap[ cur ] = d->heap[ child ];
    cur = child;
    child = _child_left(cur);
  }

  // Put last element into hole...
  uint index = d->heap[ --d->size ];
  uint val = _value(index);
  uint parent = _parent(cur);

  // ...and let it bubble up
  while( cur && val < _value(parent) )
  {
    d->heap[ cur ] = d->heap[ parent ];

    cur = parent;
    parent = _parent(cur);
  }
  
  d->heap[ cur ] = index;
  return front;
}

//------------------------------------------------------------------------------
// Dijkstra
//------------------------------------------------------------------------------
float getPenalty(read_only image2d_t costmap, int x, int y)
{
  const sampler_t sampler = CLK_FILTER_NEAREST
                          | CLK_NORMALIZED_COORDS_FALSE
                          | CLK_ADDRESS_CLAMP_TO_EDGE;

  return read_imagef(costmap, sampler, (int2)(x, y)).x;
}

//------------------------------------------------------------------------------
// Kernel
//------------------------------------------------------------------------------

#define makeIndex(pos, dim) (pos.y * dim.x + pos.x)

__kernel void test_kernel( read_only image2d_t costmap,
                           global uint* nodes,
                           global uint* queue,
                           const int2 dim,
                           const int2 start,
                           const int2 target )
{
  data_t data = {
    .values = nodes,
    .heap = queue,
    .size = 0,
    .dim = dim
  };
  
  const float link_width = 4,
              alpha_L = 0.01,
              alpha_P = 2.99,
              // simplify and scale the cost calculation a bit...
              factor = 100 * (0.5/alpha_L * alpha_P * link_width + 1);


  // Initialize all to "Not visited" and startnode to zero
  for(uint i = 0; i < dim.x * dim.y; ++i)
    nodes[i] = VALUE_NOT_VISITED;

  uint start_index = makeIndex(start, dim);
  nodes[ start_index ] = 0;
  heap_insert(&data, start_index);
  int count = 10;
  uint max_size = 0;
  // Start with Dijkstra    
  do
  {
    max_size = max(data.size, max_size);
    uint index = heap_removeMin(&data);
    int px = index % dim.x, // DON'T use int2 -> don't know why but % doesn't work...
        py = index / dim.x;

    // Update all 8 neighbours
    for( int x = max(px - 1, 0); x <= min(px + 1, dim.x); ++x )
      for( int y = max(py - 1, 0); y <= min(py + 1, dim.y); ++y )
      {
        if( px == x && py == y )
          continue;

        int index_n = makeIndex((int2)(x, y), dim);

        if( nodes[index_n] == VALUE_NOT_VISITED )
          heap_insert(&data, index_n);

        float len = (x == px || y == py) ? 1 : 1.4f;
        float cost = factor
                   * len
                   * (getPenalty(costmap, x, y) + getPenalty(costmap, px, py));

        int total_cost = nodes[index] + (int)cost;
        
        // update/relax
        if( total_cost < nodes[index_n] )
        {
          nodes[index_n] = total_cost;
        }
      }
  } while( data.size && count-- );
  
  nodes[ 0 ] = max_size;
}

#endif