
//#define KEY_TYPE float
//#define VALUE_TYPE uint
//#include "sorting.cl"



//QUEUE
typedef struct _QueueElement
{
  int blockx,
      blocky,
      blockz;
  int state;
} QueueElement;
//typedef int4 QueueElement;


#define FIXEDPRECISSIONBITS 7
#define NOATOMICFLOAT 1
#define PRECISSIONMULTIPLIER (1 << FIXEDPRECISSIONBITS)
#define MAGICPRIORITYF 999999.0f
#define MAGICPRIORITY 0x7FFFFFFF

#define QueueElementCopySteps 3

#define  QueueElementEmpty 0
#define  QueueElementReady 1
#define  QueueElementReading 2
#define  QueueElementRead 0
void checkAndSetQueueState(volatile global QueueElement* element, uint state, uint expected)
{
  //volatile global int* pstate = ((volatile global int*)element) + 3;
  //while(atomic_cmpxchg(pstate, expected, state) != expected);
  while(atomic_cmpxchg(&element->state, expected, state) != expected);
}
void setQueueState(volatile global QueueElement* element, uint state)
{
  //volatile global int* pstate = ((volatile global int*)element) + 3;
  //uint old = atomic_xchg(pstate, state);
  atomic_xchg(&element->state, state);
  //check: old + 1 % (QueueElementReading+1) == state
}
uint getQueueState(volatile global QueueElement* element)
{
  return element->state;
  //return element->w;
}
void waitForQueueState(volatile global QueueElement* element, uint expected)
{
  while(getQueueState(element) != expected);
}

int3 getBlockId(volatile global QueueElement* element)
{
  return (int3)(element->blockx, element->blocky, element->blockz);
  //return (*element).xyz;
}

void setBlockId(volatile global QueueElement* element, int3 block)
{
  element->blockx = block.x;
  element->blocky = block.y;
  element->blockz = block.z;
  //(*element).xyz = block;
}

typedef struct _QueueGlobal
{
  uint front;
  uint back;
  int filllevel;
  uint activeBlocks;
  uint processedBlocks;
  int debug;
} QueueGlobal;

#define linAccess3D(id, dim) \
  ((id).x + ( (id).y + (id).z  * (dim.y) )* (dim).x)

float atomic_min_float(volatile global float* p, float val)
{
  return as_float(atomic_min((volatile global uint*)(p),as_uint(val)));
}
float atomic_min_float_local(volatile local float* p, float val)
{
  return as_float(atomic_min((volatile local uint*)(p), as_uint(val)));
}

void setPriorityEntry(global float* p)
{
#if NOATOMICFLOAT
  *(global uint*)(p) = MAGICPRIORITY;
#else
  *p = MAGICPRIORITYF;
#endif
}
void removePriorityEntry(volatile global float* p)
{
#if NOATOMICFLOAT
  atomic_xchg((volatile global uint*)(p), MAGICPRIORITY);
#else
  atomic_xchg(p, MAGICPRIORITYF);
#endif
}
bool updatePriority(volatile global float* p, float val)
{
#if NOATOMICFLOAT
  uint nval = val * PRECISSIONMULTIPLIER;
  uint v = atomic_min((volatile global uint*)(p),nval);
  return v != MAGICPRIORITY;
#else
  float old_priority = atomic_min_float(p, val);
  return old_priority != MAGICPRIORITYF;
#endif
}


void Enqueue(volatile global QueueGlobal* queueInfo, 
             volatile global QueueElement* queue,
             volatile global float* queuePriority,
             int3 block,
             float priority,
             int2 blocks,
             uint queueSize)
{
  //check if element is already in tehe Queue:
  
  volatile global float* myQueuePriority = queuePriority + linAccess3D(block, blocks);

  //update priority:
  bool hadPriority = updatePriority(myQueuePriority, priority);
  
  //if there is a priority, it is in the queue
  if(hadPriority)
    return;
  else
  {
    //add to queue

    //inc size 
    int filllevel = atomic_inc(&queueInfo->filllevel);
    
    //make sure there is enough space?
    //assert(filllevel < queueSize)

    //insert a new element
    uint pos = atomic_inc(&queueInfo->back)%queueSize;

    //make sure it is free?
    waitForQueueState(queue + pos, QueueElementEmpty);
    //queue[pos] == QueueElementEmpty

    //insert the data
    setBlockId(queue + pos, block);
    mem_fence(CLK_GLOBAL_MEM_FENCE);
    
    //increase the number of active Blocks
    atomic_inc(&queueInfo->activeBlocks);

    //activate the queue element
    setQueueState(queue + pos,  QueueElementReady);

  }
}

bool Dequeue(volatile global QueueGlobal* queueInfo, 
             volatile global QueueElement* queue,
             volatile global float* queuePriority,
             int3* element,
             int2 blocks,
             uint queueSize)
{
  //is there something to get?
  if(atomic_dec(&queueInfo->filllevel) <= 0)
  {
    atomic_inc(&queueInfo->filllevel);
    return false;
  }

  //pop the front of the queue
  uint pos = atomic_inc(&queueInfo->front)%queueSize;

  ////check for barrier
  //uint back = queueInfo->back%queueSize;
  //while(barrierPos <= pos && barrierPos > back)
  //{
  //  barrierPos = queueInfo->sortingBarrier;
  //  queueInfo->debug += 100000;
  //}
  
  //wait for the element to be written/change to reading
  checkAndSetQueueState(queue + pos, QueueElementReading, QueueElementReady);
  
  //read
  int3 blockId = getBlockId(queue + pos);
  read_mem_fence(CLK_GLOBAL_MEM_FENCE);
  
  //remove priority (mark as not in queue)
  volatile global float* myQueuePriority = queuePriority + linAccess3D(blockId, blocks);
  removePriorityEntry(myQueuePriority);

  //free queue element
  setQueueState(queue + pos, QueueElementRead);
  *element = blockId;
  return true;
}
//

//prefix sum
//elements needs to be power of two and local_size(0) >= elements/2, data must be able to hold one more element
uint scanLocalWorkEfficient(__local uint* data, size_t linId, size_t elements)
{
  //up
  size_t offset = 1;
  for(size_t d = elements / 2; d > 0; d /= 2)
  {
    barrier(CLK_LOCAL_MEM_FENCE);
    if(linId < d)
    {
      size_t ai = offset*(2*linId+1)-1;
      size_t bi = offset*(2*linId+2)-1;
      data[bi] += data[ai];
    }
    offset *= 2;
  }
  //write out and clear last
  if (linId)
  {
    data[elements] = data[elements - 1];
    data[elements - 1] = 0;
  }
  
  //down
  for(size_t d = 1; d < elements; d *= 2)
  {
    offset /= 2;
    barrier(CLK_LOCAL_MEM_FENCE);
    if(linId < d)
    {
      size_t ai = offset*(2*linId+1)-1;
      size_t bi = offset*(2*linId+2)-1;
      uint t = data[ai];
      data[ai] = data[bi];
      data[bi] += t;
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  return data[elements];
}


//

float getPenalty(read_only image2d_t costmap, int2 pos)
{
  const sampler_t sampler = CLK_FILTER_NEAREST
                          | CLK_NORMALIZED_COORDS_FALSE
                          | CLK_ADDRESS_CLAMP_TO_EDGE;

  return read_imagef(costmap, sampler, pos).x;
}

float readLastPenalty(global const float* lastCostmap, int2 pos, const int2 size)
{
  return lastCostmap[pos.x + pos.y*size.x];
}

void writeLastPenalty(global float* lastCostmap, int2 pos, const int2 size, float data)
{
  lastCostmap[pos.x + pos.y*size.x] = data;
}

int borderId(int2 lid, int2 lsize)
{
  int id = -1;
  if(lid.y == 0)
    id = lid.x;
  else if(lid.x==lsize.x-1)
    id = lid.x + lid.y;
  else if(lid.y==lsize.y-1)
    id = lsize.x*2 + lsize.y - 3 - lid.x;
  else if(lid.x == 0 && lid.y > 0)
    id = 2*(lsize.x + lsize.y - 2) - lid.y;
  return id;
}

int2 lidForBorderId(int borderId, int2 lsize)
{
  if(borderId == -1 || borderId > 2*(lsize.x + lsize.y - 2))
    return (int2)(-1,-1);
  if(borderId <  lsize.x)
    return (int2)(borderId, 0);
  if(borderId < lsize.x + lsize.y - 1)
    return (int2)(lsize.x-1, borderId - (lsize.x-1));
  if(borderId < 2*lsize.x + lsize.y - 2)
    return (int2)(2*lsize.x+lsize.y-3-borderId , lsize.y-1);
  else
    return (int2)(0, 2*(lsize.x + lsize.y - 2) - borderId);
}

int dir4FromBorderId(int borderId, int2 lsize)
{
  int borderElements = 2*(lsize.x+lsize.y-2);
  if(borderId < 0 || borderId >= borderElements) return 0;
  
  //int mask = 0;
  //if(borderId < lsize.x) mask |= 0x1;
  //if(borderId >= lsize.x-1 && borderId < lsize.x+lsize.y-1 ) mask |= 0x2;
  //if(borderId >= lsize.x+lsize.y-2 && borderId < 2*lsize.x+lsize.y-2 ) mask |= 0x4;
  //if(borderId >= 2*lsize.x+lsize.y-3 || borderId == 0 ) mask |= 0x8;
  int mask =  (borderId < lsize.x) | 
              ((borderId >= lsize.x-1 && borderId < lsize.x+lsize.y-1 ) << 1) |
              ((borderId >= lsize.x+lsize.y-2 && borderId < 2*lsize.x+lsize.y-2 ) << 2) |
              ((borderId >= 2*lsize.x+lsize.y-3 || borderId == 0) << 3);
  return mask;
}

int dir8FromBorderId(int borderId, int2 lsize)
{
  if(borderId < 0) return 0;
  int mask =  (borderId == 0) | 
              ((borderId < lsize.x) << 1) |
              ((borderId == lsize.x-1) << 2) |
              ((borderId >= lsize.x-1 && borderId < lsize.x+lsize.y-1) << 3) |
              ((borderId ==  lsize.x+lsize.y-2) << 4) |
              ((borderId >= lsize.x+lsize.y-2 && borderId < 2*lsize.x+lsize.y-2) << 5) |
              ((borderId == 2*lsize.x+lsize.y-3) << 6) |
              ((borderId >= lsize.x+lsize.y-3) << 7);
  return mask;
}

int getCostGlobalId(const int lid, const int2 blockId, const int2 blockSize, const int2 blocks)
{
  int rowelements = (blockSize.x - 1) * blocks.x + 1;
  int colelements = (blockSize.y - 2) * (blocks.x + 1);
  int myoffset = blockId.y * (rowelements + colelements);

  if(lid < 0)
    return -1;
  if(lid < blockSize.x)
    return myoffset + blockId.x*(blockSize.x - 1) + lid;
  if(lid < blockSize.x + blockSize.y - 2)
    return myoffset + rowelements + (blockId.x+1)*(blockSize.y - 2) + lid - blockSize.x;
  if(lid < 2*blockSize.x + blockSize.y - 2)
    return myoffset + rowelements + colelements + blockId.x*(blockSize.x - 1)   + 2*blockSize.x+blockSize.y-3 - lid;
  else
    return myoffset + rowelements + blockId.x*(blockSize.y - 2)    + 2*(blockSize.x+blockSize.y-2)-1 - lid;
}


local float* accessLocalCost(local float* l_costs, int2 id)
{
  return l_costs + (id.x+1 + (id.y+1)*(get_local_size(0)+2));
}

local const float* accessLocalCostConst(local float const * l_costs, int2 id)
{
  return l_costs + (id.x+1 + (id.y+1)*(get_local_size(0)+2));
}

bool route_data(local float const * l_cost,
                local float* l_route)
{
  //route as long as there is change
  local bool change;
  change = true;
  barrier(CLK_LOCAL_MEM_FENCE);

  int2 localid = (int2)(get_local_id(0),get_local_id(1));
  local float* myval = accessLocalCost(l_route, localid);
  //iterate over it
  int counter = -1;
  while(change)
  {
    change = false;
    barrier(CLK_LOCAL_MEM_FENCE);

    float lastmyval = *myval;
    
    int2 offset;
    for(offset.y = -1; offset.y  <= 1; ++offset.y)
      for(offset.x = -1; offset.x <= 1; ++offset.x)
      {
        //d is either 1 or M_SQRT2 - maybe it is faster to substitute
        //float d = native_sqrt((float)(offset.x*offset.x+offset.y*offset.y));
        //by
        int d2 = offset.x*offset.x+offset.y*offset.y;
        float d = (d2-1)*M_SQRT2 + (2-d2);
        float penalty = 0.5f*(*accessLocalCostConst(l_cost, localid) +
                              *accessLocalCostConst(l_cost, localid + offset));
        float r_other = *accessLocalCost(l_route, localid + offset);
        //TODO use custom params here
        *myval = min(*myval, r_other + 0.01f*d + d*penalty);
      }

    if(*myval != lastmyval)
      change = true;    
    
    barrier(CLK_LOCAL_MEM_FENCE);
    ++counter;
  }

  return counter > 0;
}


float loadCostToLocalAndSetRoute(const int2 group_id, const int2 gid, const int2 dim, global const float* g_cost, local float* l_cost, local float* l_route)
{
  int lid = get_local_id(0) + get_local_id(1)*get_local_size(0);
  for(; lid < (get_local_size(0)+2)*(get_local_size(1)+2); lid += get_local_size(0)*get_local_size(1))
  {
    l_cost[lid] = 0.1f*MAXFLOAT;
    l_route[lid] = 0.1f*MAXFLOAT;
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  float r_in = 0.1f*MAXFLOAT;
  if(gid.x < dim.x && gid.y < dim.y)
  {
    r_in = readLastPenalty(g_cost, gid, dim);
    *accessLocalCost(l_cost, (int2)(get_local_id(0), get_local_id(1))) = r_in;
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  return r_in;
}

__kernel void updateRouteMap(read_only image2d_t costmap,
                             global float* lastCostmap,
                             global float* routeMap,
                             const int2 dim,
                             int computeAll,
                             local float* l_data)
{
  local bool changed;
  changed = computeAll>0?true:false;

  local float* l_cost = l_data;
  local float* l_routing_data = l_data + (get_local_size(0)+2)*(get_local_size(1)+2);

  int2 gid2 = (int2)(get_group_id(0)*(get_local_size(0)-1) + get_local_id(0), get_group_id(1)*(get_local_size(1)-1) + get_local_id(1));

  
  float newcost = getPenalty(costmap, (int2)(gid2.x, gid2.y));
  float last_cost = loadCostToLocalAndSetRoute((int2)(get_group_id(0), get_group_id(1)), gid2, dim, lastCostmap, l_cost, l_routing_data);

  if(gid2.x < dim.x && gid2.y < dim.y && (computeAll || fabs(last_cost - newcost) > 0.01f) ) 
  {
    writeLastPenalty(lastCostmap, gid2, dim, newcost);
    *accessLocalCost(l_cost, (int2)(get_local_id(0), get_local_id(1))) = newcost;
    changed = true;
  }

  barrier(CLK_LOCAL_MEM_FENCE);
  if(!changed) return;


  //do the routing
  int boundaryelements = 2*(get_local_size(0) + get_local_size(1)-2);
  int written = 0;
  int myBorderId = borderId((int2)(get_local_id(0), get_local_id(1)), (int2)(get_local_size(0),get_local_size(1)));
  for(int i = 0; i < boundaryelements; ++i)
  {
    //init routing data
    float mydata = 0.1f*MAXFLOAT;
    if(i == myBorderId)
      mydata = 0.0f;
    *accessLocalCost(l_routing_data, (int2)(get_local_id(0), get_local_id(1))) = mydata;

    //route
    route_data(l_cost, l_routing_data);

    //write result
    if(myBorderId >= i) 
      routeMap[boundaryelements*boundaryelements/2*(get_group_id(0)+get_group_id(1)*get_num_groups(0)) +
               written + myBorderId - i] = *accessLocalCost(l_routing_data, (int2)(get_local_id(0), get_local_id(1)));
    written += boundaryelements - i;
  }
}



__kernel void prepareBorderCosts(global float* routedata)
{
  routedata[get_global_id(0) + get_global_size(0)*get_global_id(1)] = 0.01f*MAXFLOAT;
}


__kernel void prepareIndividualRouting(const global float* costmap,
                                       global float* routedata,
                                       const global uint4* startingPoints,
                                       const global uint4* blockToDataMapping,
                                       const int2 dim,
                                       const int2 numBlocks,
                                       const int routeDataPerNode,
                                       local float* l_data)
{
  uint4 mapping = blockToDataMapping[get_group_id(0)];


  int2 gid = (int2)(mapping.x*(get_local_size(0)-1)+get_local_id(0),
                    mapping.y*(get_local_size(1)-1)+get_local_id(1));


  float startcost = 0.01f*MAXFLOAT;

  if(gid.x < dim.x && gid.y < dim.y)
  {
    if(startingPoints[mapping.z].x <= gid.x && gid.x <= startingPoints[mapping.z].z &&
       startingPoints[mapping.z].y <= gid.y && gid.y <= startingPoints[mapping.z].w)
        startcost = 0;
  }

  local float* l_cost = l_data;
  local float* l_route = l_data + (get_local_size(0)+2)*(get_local_size(1)+2);

  //local routing
  loadCostToLocalAndSetRoute((int2)(mapping.x, mapping.y), gid, dim, costmap, l_cost, l_route);
  barrier(CLK_LOCAL_MEM_FENCE);
  *accessLocalCost(l_route, (int2)(get_local_id(0),get_local_id(1))) = startcost;
  //route
  route_data(l_cost, l_route);


  int lid =  borderId((int2)(get_local_id(0),get_local_id(1)), (int2) (get_local_size(0), get_local_size(1)));
  if(lid >= 0)
  {
    //write out result
    int offset = getCostGlobalId(lid, (int2)(mapping.x, mapping.y), (int2) (get_local_size(0), get_local_size(1)), numBlocks);
    routedata[routeDataPerNode*mapping.w + offset] = *accessLocalCost(l_route, (int2)(get_local_id(0),get_local_id(1)));
  }
}

float getRouteMapCost(global const float* ourRouteMapBlock, int from, int to, const int borderElements)
{
  int row = min(from,to);
  int col = max(from,to);
  col = col - row;
  return ourRouteMapBlock[col + row*borderElements - row*(row-1)/2];
}

int routeBorder(global const float* routeMap, 
                volatile global float* routingData,
                const int2 blockId, 
                const int2 blockSize,
                const int2 numBlocks,
                local float* routeData,
                local int* changeData)
{
  int lid = get_local_id(0);
  int lsize = get_local_size(0);
  int borderElements = 2*(blockSize.x + blockSize.y - 2);
  int gid = getCostGlobalId(lid, blockId, blockSize, numBlocks);

  *changeData = 0;
  float mycost = 0.01f*MAXFLOAT;
  global const float* ourRouteMap = routeMap + borderElements*borderElements/2*(blockId.x + blockId.y*numBlocks.x);
  if(lid < borderElements)
  {    
    mycost = routingData[gid];
    routeData[lid] = mycost;
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  if(lid < borderElements)
  {
    float origCost = mycost;
    for(int i = 0; i < borderElements; ++i)
    {
      float newcost = routeData[i] + getRouteMapCost(ourRouteMap, i, lid, borderElements);
      mycost = min(newcost, mycost);
    }
    
    if(mycost < origCost)
    {
      //atomic as multiple might be working on the same
      atomic_min((volatile global unsigned int*)(routingData + gid), as_uint(mycost));
      routeData[lid] = mycost;
      
      //TODO: this is already an extension for opencl 1.0 so maybe do them individually...
      atomic_or(changeData,dir4FromBorderId(lid, blockSize));
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  return *changeData;
}

uint createDummyQueueEntry()
{
  return 0xFFFFFFFF;
}
uint createQueueEntry(const int2 block, const float priority, const uint minPriorty)
{
  return (max(minPriorty,min((uint)(priority*0.5f),0xFFFu)) << 20) |
         (block.y << 10) | block.x;
}
int2 readQueueEntry(const uint entry)
{
  return (int2)(entry & 0x3FFu, (entry >> 10) & 0x3FFu);
}
bool compareQueueEntryBlocks(const uint entry1, const uint entry2)
{
  return (entry1&0xFFFFFu)==(entry2&0xFFFFFu);
}

uint4 nextBlockRange(uint4 dims, uint4 limits)
{
  uint maxuint = 0xFFFFFFFF;
  uint current =  maxuint - max(max(dims.x, dims.y), max(dims.z, dims.w)) + 1;
  //get next highest
  uint next = max(max(dims.x+current,dims.y+current),max(dims.z+current,dims.w+current));
  next = (next != 0)*(next - current);
  return min(limits,(uint4)(next,next,next,next));
}
uint popc(uint val)
{
  uint c = 0;
  for(uint i = 0; i < 32; ++i, val >>= 1)
    c += val & 0x1;
  return c;
}
int2 activeBlockToId(const int2 seedBlock, const int spiralIdIn, const int2 activeBlocksSize)
{
  int spiralId = spiralIdIn/2;
  //get limits
  int2 seedAB = (int2)(seedBlock.x/8, seedBlock.y/8);
  uint4 spiral_limits = (uint4)( seedAB.x+1,  seedAB.y+1,
    activeBlocksSize.x - seedAB.x, activeBlocksSize.y - seedAB.y);
  uint4 area_dims = spiral_limits;

  int2 resId = seedAB;
  for(int i = 0; i < 4; ++i)
  {
    area_dims = nextBlockRange(area_dims, spiral_limits);
    int w = max(1, (int)(area_dims.x + area_dims.z - 1));
    int h = max(1, (int)(area_dims.y + area_dims.w - 1));
    if(w*h <= spiralId)
    {
      //we are outside of this box
      
      //where do we increase
      uint increase_x = (area_dims.x != spiral_limits.x) + (area_dims.z != spiral_limits.z);
      uint increase_y = (area_dims.y != spiral_limits.y) + (area_dims.w != spiral_limits.w);

      
      int outerspirals = 0;

      float c = -(spiralId-w*h);
      float b = increase_x*h + increase_y*w;
      float a = increase_x*increase_y;

      if(a != 0)
      {
        float sq = sqrt(b*b - 4*a*c);
        outerspirals = (-b + sq)/(2*a);
      }
      else
        outerspirals = - c / b;

      int innerspirals = max(1U,max(max(area_dims.x, area_dims.y),max(area_dims.z, area_dims.w)));
      int4 allspirals = (int4)(max(0,seedAB.x-innerspirals-outerspirals+1), max(0,seedAB.y-innerspirals-outerspirals+1), 
                             min(activeBlocksSize.x-1,seedAB.x+innerspirals+outerspirals-1), min(activeBlocksSize.y-1,seedAB.y+innerspirals+outerspirals-1));
      int allspiralelements = (allspirals.z - allspirals.x+1)*(allspirals.w - allspirals.y+1);
      int myoffset = spiralId - allspiralelements;

      //find the right position depend on the offset
      if(allspirals.y > 0)
      {
        int topelements = allspirals.z - allspirals.x + 1 + (allspirals.z < activeBlocksSize.x-1);
        if(myoffset < topelements)
        {
          resId = (int2)(allspirals.x + myoffset, allspirals.y-1);
          break;
        }
        else
          myoffset -= topelements;
      }
      if(allspirals.z < activeBlocksSize.x-1)
      {
        int rightelements = allspirals.w - allspirals.y + 1 + (allspirals.w < activeBlocksSize.y-1);
        if(myoffset < rightelements)
        {
          resId = (int2)(allspirals.z + 1, allspirals.y + myoffset);
          break;
        }
        else
          myoffset -= rightelements;
      }
      if(allspirals.w < activeBlocksSize.y-1)
      {
        int bottomelements = allspirals.w - allspirals.y + 2;
        if(myoffset < bottomelements)
        {
          resId = (int2)(allspirals.z - myoffset, allspirals.w + 1);
          break;
        }
        else
          myoffset -= bottomelements;
      }
      //else
      resId = (int2)(allspirals.x - 1, allspirals.w - myoffset);
      break;
    }
  }
  return (int2)(resId.x*8, resId.y*8 + (spiralIdIn%1)*4);
}

int2 getActiveBlock(const int2 seedBlock, const int2 blockId, const int2 activeBlocksSize)
{
  int2 seedAB = (int2)(seedBlock.x/8, seedBlock.y/8);
  int2 blockAB = (int2)(blockId.x/8, blockId.y/8);
  //get full spiral count
  int spirals = max(max(seedAB.x - blockAB.x, blockAB.x - seedAB.x), max(seedAB.y - blockAB.y, blockAB.y - seedAB.y));

  int offset = 0;
  int2 diff = blockAB - seedAB;
  
  //top row
  if(diff.y == -spirals && diff.x > -spirals)
  {
    int4 dims;
    dims.x = max(0, seedAB.x - spirals + 1);
    dims.z = min(activeBlocksSize.x - 1, seedAB.x + spirals - 1);
    dims.y = seedAB.y - spirals + 1;
    dims.w = min(activeBlocksSize.y - 1, seedAB.y + spirals - 1);
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      blockAB.x - dims.x;
  }
  //right side
  else if(diff.x == spirals)
  {
    int4 dims;
    dims.x = max(0, seedAB.x - spirals + 1);
    dims.z = seedAB.x + spirals - 1;
    dims.y = max(0, seedAB.y - spirals);
    dims.w = min(activeBlocksSize.y - 1, seedAB.y + spirals - 1);
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      blockAB.y - dims.y;
  }
  //bottom side
  else if(diff.y == spirals)
  {
    int4 dims;
    dims.x = max(0, seedAB.x - spirals + 1);
    dims.z = min(activeBlocksSize.x - 1, seedAB.x + spirals);
    dims.y = max(0, seedAB.y - spirals);
    dims.w = seedAB.y + spirals - 1;
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      dims.z - blockAB.x;
  }
  //left side
  else
  {
    int4 dims;
    dims.x = seedAB.x - spirals + 1;
    dims.z = min(activeBlocksSize.x - 1, seedAB.x + spirals);
    dims.y = max(0, seedAB.y - spirals);
    dims.w = min(activeBlocksSize.y - 1, seedAB.y + spirals);
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      dims.w - blockAB.y;
  }

  int2 interBlock = (int2)(blockId.x%8, blockId.y%8);
  int second = interBlock.y > 4;
  interBlock.y -= second*4;
  int mask = 1 << (interBlock.x + interBlock.y*8);
  return (int2)(2*offset + second, mask);
}
void unsetActiveBlock(volatile global uint* myActiveBlock, const int2 seedBlock, const int2 blockId, const int2 activeBlocksSize)
{
  int2 tblock = getActiveBlock(seedBlock, blockId, activeBlocksSize);
  atomic_and(myActiveBlock + tblock.x, ~tblock.y);
}
void setActiveBlock(volatile global uint* myActiveBlock, const int2 seedBlock, const int2 blockId, const int2 activeBlocksSize)
{
  int2 tblock = getActiveBlock(seedBlock, blockId, activeBlocksSize);
  atomic_or(myActiveBlock + tblock.x, tblock.y);
}

__kernel void routeLocal(global const float* routeMap,
                         global const int4* seed,
                         global const int* routingIds,
                         volatile global float* routingData,
                         const int routeDataPerNode,
                         const int2 blockSize,
                         const int2 numBlocks,
                         local int* data,
                         local uint* queue,
                         const int queueSize,
                         volatile global uint* activeBuffer,
                         const int2 activeBufferSize)
{
  int borderElements = 2*(blockSize.x + blockSize.y - 2);
  int workerId = get_local_id(1);
  const int lid = get_local_id(0);
  const int linId = get_local_id(0) + get_local_size(0)*get_local_id(1);
  const int workerSize = get_local_size(0);
  const int workers = get_local_size(1);
  int withinWorkerId = get_local_id(0);
  int elementsPerWorker = borderElements + 1;
  routingData = routingData + routeDataPerNode*routingIds[get_group_id(0)];
  
  local float* l_routeData = (local float*)(data + elementsPerWorker*workerId);
  local int* l_changedData = (local int*)(l_routeData + borderElements);



  //init queue
  int4 ourInit = seed[get_group_id(0)];
  int queueFront = 0;
  int queueElements = (ourInit.w-ourInit.y)*(ourInit.z-ourInit.x);
  int posoffset = (ourInit.z-ourInit.x)*get_local_id(1);
  int2 seedBlock = (int2)((ourInit.x+ourInit.z)/2, (ourInit.y+ourInit.w)/2);
  for(int y = ourInit.y + get_local_id(1); y < ourInit.w; y+= get_local_size(1))
  {
    for(int x = get_local_id(0); x < (ourInit.z - ourInit.x); x += get_local_size(0))
    {
      int pos = posoffset + x;
      queue[pos] = createQueueEntry((int2)(ourInit.x+x, y), 0, 0);       
    }
    posoffset += (ourInit.z-ourInit.x);
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  while(queueElements)
  {
    //do the work
    while(queueElements)
    {
      int changed = 0;
      int2 blockId;
      if(workerId < queueElements)
      {
        //there is work for me to be done
        blockId = readQueueEntry(queue[(queueFront + workerId)%queueSize]);
        changed = routeBorder(routeMap, 
                  routingData, 
                  blockId,
                  blockSize,
                  numBlocks,
                  l_routeData,
                  l_changedData);
        //unsetActiveBlock(activeBuffer + 2*activeBufferSize.x*activeBufferSize.y*get_group_id(0), seedBlock, blockId, activeBufferSize);
      }
      int nowWorked = min(queueElements, workers);
      queueFront += nowWorked;
      queueElements -= nowWorked;

      //queue management (one after the other)
      local uint l_queueElement;
      local uint hasElements;
      for(uint w = 0; w < nowWorked; ++w)
      {
        barrier(CLK_LOCAL_MEM_FENCE);
        if(w == workerId)
          hasElements = changed;
        for(int element = 0; element < 8; ++element)
        {
          barrier(CLK_LOCAL_MEM_FENCE);
          if((hasElements & (1 << element)) == 0)
            continue;
          l_queueElement = 0xFFFFFFFF;
          barrier(CLK_LOCAL_MEM_FENCE);
          //generate new entry
          if(w == workerId && ((1 << element) & dir4FromBorderId(lid,blockSize)))
          {
            //int2 offset = (int2)(0,0);
            //if(element == 0 || element >= 6) offset.x = -1;
            //else if(element >= 2 && element <= 4) offset.x = 1;
            //if(element >= 0 && element <= 2) offset.y = -1;
            //else if(element >= 5 && element <= 6) offset.y = 1;
            int2 offset = (int2)(element>0?2-element:0, element<3?element-1:0);
            int2 newId = blockId + offset;
            if(newId.x >= 0 && newId.y >= 0 &&
                newId.x < numBlocks.x && newId.y < numBlocks.y)
              atomic_min(&l_queueElement, createQueueEntry(newId, l_routeData[lid],1));
          }

          barrier(CLK_LOCAL_MEM_FENCE);
          if(l_queueElement == 0xFFFFFFFF)
            continue;
          //insert new entry
          --queueFront;
          queueFront = (queueFront + queueSize) % queueSize;

          
          if(queueElements == queueSize)
          {
            //kick out last
            queueElements = queueSize-1;
            //set as active for later
            if(linId == 0)
              setActiveBlock(activeBuffer + 2*activeBufferSize.x*activeBufferSize.y*get_group_id(0), seedBlock, queue[queueFront], activeBufferSize);
          }
          
          uint newEntry = l_queueElement;
          barrier(CLK_LOCAL_MEM_FENCE);
          
          if(linId == 0)
          {
            l_queueElement = queueSize;
            queue[queueFront] = newEntry;
          }
          
          for(int i=0; i < queueElements; i+=get_local_size(0)*get_local_size(1))
          {
            int tId = linId + i;
            uint nextEntry;
            if(tId < queueElements)
            {
              nextEntry = queue[(queueFront + tId + 1)%queueSize];
              //did we find the entry?
              if(compareQueueEntryBlocks(nextEntry, newEntry))
                l_queueElement = tId;
            }
          
            barrier(CLK_LOCAL_MEM_FENCE);
            if(tId < queueElements)
            {
              //if new element has higher cost -> copy old back
              //if same element has been found -> need to copy all back
              if(nextEntry < newEntry || tId > l_queueElement)
                queue[(queueFront + tId)%queueSize] = nextEntry;
              //insert the new element, if my old one is the last to be copied away
              else if(queue[(queueFront + tId)%queueSize] < newEntry)
                queue[(queueFront + tId)%queueSize] = newEntry;
            }
          }

          barrier(CLK_LOCAL_MEM_FENCE);
          //new element added
          if(l_queueElement == queueSize)
          {
            //element must be added at the back...
            if(queue[(queueFront + queueElements)%queueSize] == queue[(queueFront + queueElements - 1)%queueSize])
              queue[(queueFront + queueElements)%queueSize] = newEntry;
            ++queueElements;
          }
        }
      }
    }

    //get back active blocks
    //to a maximum of queueSize/4 elements
    queueFront = 0;
    uint pullers = min(queueSize/2, (int)(get_local_size(0)*get_local_size(1)) );
    int pullRun = 0;
    int pullId = linId;
    while(queueElements < queueSize/4 && pullRun * pullers < 2*activeBufferSize.x*activeBufferSize.y)
    {
      uint myentry = 0;
      uint canOffer = 0;
      if(linId < pullers)
      {
        if(pullId < 2*activeBufferSize.x*activeBufferSize.y)
        {
          myentry = activeBuffer[2*activeBufferSize.x*activeBufferSize.y*get_group_id(0) + pullId];
          canOffer = popc(myentry);
        }
        queue[queueSize/2 + linId] = canOffer;
      }
      barrier(CLK_LOCAL_MEM_FENCE);
      
      //run prefix sum
      scanLocalWorkEfficient(queue + queueSize/2, linId, pullers);

      //put into queue
      if(myentry != 0)
      {
        int myElements = min(canOffer, queueSize/4 - (queueElements + queue[queueSize/2 + linId]));
        int2 myOffset = activeBlockToId(seedBlock, pullId, activeBufferSize);

        int2 additionalOffset = (int2)(0,0);
        for(int inserted = 0; additionalOffset.y < 4; ++additionalOffset.y)
          for(additionalOffset.x = 0; additionalOffset.x < 8 && inserted < myElements; ++additionalOffset.x, myentry >>= 1)
          {
            if(myentry & 0x1)
            {
              queue[queueElements + queue[queueSize/2 + linId] + inserted] = createQueueEntry(myOffset + additionalOffset , 0, 0);
              //debug
              //activeBuffer[queueElements + queue[queueSize/2 + linId] + inserted] = createQueueEntry(myOffset + additionalOffset , 0, 0);
              ++inserted;
            }
          }
      }
      queueElements =  min((int)(queueElements + queue[queueSize/2 + pullers]), queueSize/4);
      barrier(CLK_LOCAL_MEM_FENCE);
      pullId += pullers;
      pullRun++;
    }
  }

 
}

__kernel void getMinimum(global const float* routecost,
                         //volatile global QueueGlobal* queueInfo,
                         volatile global ushort2* coordinates,
                         const int2 dim,
                         int targets)
{
  //float cost = 0;
  //for(int i = 0; i < targets; ++i)
  //  cost += routecost[dim.x*get_global_id(1) + get_global_id(0) + dim.x*dim.y*i];

  //__local uint localmin;
  //__local uint2 coords;
  //localmin = as_uint(MAXFLOAT);

  //barrier(CLK_LOCAL_MEM_FENCE);

  //atomic_min(&localmin, as_uint(cost));

  //barrier(CLK_LOCAL_MEM_FENCE);

  //if(localmin == as_uint(cost))
  //  coords = (uint2)(get_global_id(0), get_global_id(1));

  //barrier(CLK_LOCAL_MEM_FENCE);

  //if( (get_local_id(0) + get_local_id(1)) == 0 )
  //{
  //  uint minv = as_uint(localmin);
  //  //compress it
  //  minv = (minv << 1) & 0xFFFFFF00;
  //  //combine with block id
  //  uint groupv = (get_group_id(0) + get_group_id(1)*get_num_groups(0)) & 0xFF;
  //  uint combined = minv | groupv;
  //  uint old = atomic_min(&queueInfo->mincost, combined);
  //  if(old > combined)
  //  {
  //    //write coords to array entry
  //    coordinates[groupv] = (ushort2)(coords.x,coords.y);
  //  }
  //}
}


__kernel void calcInOut(global const float* routecost,
                        global uint* blockroutes,
                        local int* l_cost,
                        local int* l_origin,
                        const int2 start,
                        const int2 dim)
{
  int3 coord = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));
  float mincost = MAXFLOAT;
  if(coord.x >= 0 && coord.x < dim.x && 
     coord.y >= 0 && coord.y < dim.y)
    mincost =  routecost[coord.x + dim.x*coord.y + dim.x*dim.y*coord.z];

  int2 dir = (int2)(0,0);
  for(int y = -1; y <= 1; ++y)
    for(int x = -1; x <= 1; ++x)
    {
      coord = (int3)(get_global_id(0)+x, get_global_id(1)+y, get_global_id(2));
      if(coord.x >= 0 && coord.x < dim.x && 
         coord.y >= 0 && coord.y < dim.y)
      {
        float tcost =  routecost[coord.x + dim.x*coord.y + dim.x*dim.y*coord.z];
        if(tcost < mincost)
        {
          mincost = tcost;
          dir = (int2)(x,y);
        }
      }
    }
  //store dir and cost
  int2 lid = (int2)(get_local_id(0), get_local_id(1));
  int2 fromid = lid + dir;
  
  bool outside = fromid.x < 0 ||
                fromid.y < 0 ||
                fromid.x >= get_local_size(0) ||
                fromid.y >= get_local_size(1);

  int cost = (dir.x == 0 && dir.y == 0) ? 0 : 
             (outside ? 1 : (2*get_local_size(0)*get_local_size(1)));

  int origin = (dir.x == 0 && dir.y == 0) ? ( ( (1 + lid.y) << 8 ) | (lid.x + 1)) :
               (outside ? ( ( (1 + fromid.y) << 8 ) | (1 + fromid.x ) ) : -1);

  
  int loffset = lid.x + lid.y*(int)get_local_size(0);
  int lfrom = fromid.x + fromid.y*(int)get_local_size(0);
  l_cost[loffset] = cost;
  l_origin[loffset] = origin;
  
  __local bool changed;
  uint count = 0;
  do
  {
    changed = false;
    barrier(CLK_LOCAL_MEM_FENCE);
    if(origin == -1)
    {
      if(l_cost[loffset] != l_cost[lfrom]+1)
      {
        l_cost[loffset] = l_cost[lfrom]+1;
        l_origin[loffset] = l_origin[lfrom];
        changed = true;
      }
    }      
    barrier(CLK_LOCAL_MEM_FENCE);
  }
  while(changed);


  bool border =  get_local_id(0) == 0 ||
                 get_local_id(1) == 0 ||
                 get_local_id(0) == get_local_size(0)-1 ||
                 get_local_id(1) == get_local_size(1)-1;
  
  uint info = (l_cost[loffset] << 16) | (l_origin[loffset] &0xFFFF);

  if(border)
  {
    //store the info
    uint localoffset = borderId((int2)(get_local_id(0),get_local_id(1)), (int2)(get_local_size(0),get_local_size(1)));    
    uint groupid = get_group_id(0)+get_group_id(1)*get_num_groups(0) + get_group_id(2)*get_num_groups(0)*get_num_groups(1);
    uint elements_per_group = 2*(get_local_size(0) + get_local_size(1) - 2);
    //blockroutes[0] = info;
    blockroutes[ get_global_size(2) + groupid * elements_per_group + localoffset] =  info;
  }

  if( start.x == get_global_id(0) && start.y == get_global_id(1) )
  {
    //this is the starting point, we also need to save this info
    blockroutes[ get_global_id(2) ] =  info;
  }
  
}


uint blockInOutCost(uint data)
{
  return data >> 16;
}
int2 blockInOutOrigin(uint data)
{
  int x = data & 0xFF;
  int y = (data >> 8) & 0xFF;
  return (int2)(x-1,y-1);
}
uint pack(int2 d)
{
  return (d.x << 16) | d.y;
}
int2 unpack(uint d)
{
  return (int2)(d >> 16, d & 0xFFFF); 
}

__kernel void calcInterBlockRoute(global const uint* blockroutes,
                                  global uint2* interblockroutes,
                                  const int2 start,
                                  const int2 blocks,
                                  const int2 blocksize)
{
  uint target = get_global_id(0);
  uint startinfo = blockroutes[target];
  uint elements_per_group = 2*(blocksize.x + blocksize.y - 2);
  uint inoffset = get_global_size(0) + target*blocks.x*blocks.y*elements_per_group;
  uint outoffset = blocks.x*blocks.y*target + get_global_size(0);
  uint blockcount = 0;

  uint newcost = blockInOutCost(startinfo);
  int2 localorigin = blockInOutOrigin(startinfo);
  interblockroutes[outoffset++] = (uint2)(0,pack(start));

  int2 block = (int2)(start.x/blocksize.x, start.y/blocksize.y);
  uint sumcost = 0;
  int2 blockdiff = (int2)(0,0);

  do
  {
    sumcost += newcost;
    int2 currentpos = block*blocksize + localorigin;
    blockdiff = (int2)(localorigin.x < 0 ? -1 : (localorigin.x >= blocksize.x ? 1 : 0),
                       localorigin.y < 0 ? -1 : (localorigin.y >= blocksize.y ? 1 : 0));
    block.x += blockdiff.x;
    block.y += blockdiff.y;
    int2 localpos = currentpos - block*blocksize;

    int localoffset = borderId(localpos, blocksize);
    interblockroutes[outoffset++] = (uint2)(sumcost,pack(currentpos));

    uint currentinfo = blockroutes[inoffset + (block.y*blocks.x + block.x)*elements_per_group + localoffset];
    newcost = blockInOutCost(currentinfo);
    localorigin = blockInOutOrigin(currentinfo);
    ++blockcount;

  } while(blockdiff.x != 0 || blockdiff.y != 0);

  interblockroutes[target].x = blockcount;
  interblockroutes[target].y = sumcost+1;
}

__kernel void constructRoute(global const float* routecost,
                             global const uint2* interblockroutes,
                             global int2* route,
                             const int maxblocks_pertarget,
                             const int maxpoints_pertarget,
                             const int numtargets,
                             const int2 dim,
                             const int2 blocks)
{
  uint target = get_global_id(0)/maxblocks_pertarget;
  uint element = get_global_id(0)%maxblocks_pertarget;
  if(element >= interblockroutes[target].x)
    return;
  
  int offset = maxpoints_pertarget*target + interblockroutes[blocks.x*blocks.y*target + numtargets + element].x;
  int endoffset = maxpoints_pertarget*target + interblockroutes[blocks.x*blocks.y*target + numtargets + element + 1].x;
  int2 pos = unpack( interblockroutes[blocks.x*blocks.y*target + numtargets + element].y );

  route[offset++] = pos;
  //follow the route
  while(offset <= endoffset)
  {
    float mincost = MAXFLOAT;
    int2 next = pos;
    for(int y = -1; y <= 1; ++y)
      for(int x = -1; x <= 1; ++x)
      {
        int2 coord = (int2)(pos.x+x, pos.y+y);
        if(coord.x >= 0 && coord.x < dim.x && 
           coord.y >= 0 && coord.y < dim.y)
        {
          float tcost =  routecost[coord.x + dim.x*coord.y + dim.x*dim.y*target];
          if(mincost > tcost)
          {
            mincost = tcost;
            next = coord;
          }
        }
      }
    pos = next;
    route[offset++] = pos;
  }
}


__kernel void initMem(__global uint* data, uint val)
{
  uint id = get_global_id(0) + get_global_id(1)*get_global_size(0);
  data[id] = val;
}


//#define KEY_TYPE float
//#define VALUE_TYPE uint
//#include "sorting.cl"
//
//
//
////QUEUE
//typedef struct _QueueElement
//{
//  int blockx,
//      blocky,
//      blockz;
//  uint state;
//} QueueElement;
////typedef int4 QueueElement;
//
//
//#define FIXEDPRECISSIONBITS 7
//#define NOATOMICFLOAT 1
//#define PRECISSIONMULTIPLIER (1 << FIXEDPRECISSIONBITS)
//#define MAGICPRIORITYF 999999.0f
//#define MAGICPRIORITY 0x7FFFFFFF
//
//#define QueueElementCopySteps 3
//
//#define  QueueElementEmpty 0
//#define  QueueElementReady 1
//#define  QueueElementReading 2
//#define  QueueElementRead 0
//void checkAndSetQueueState(volatile global QueueElement* element, uint state, uint expected)
//{
//  //volatile global int* pstate = ((volatile global int*)element) + 3;
//  //while(atomic_cmpxchg(pstate, expected, state) != expected);
//  while(atomic_cmpxchg(&element->state, expected, state) != expected);
//}
//void setQueueState(volatile global QueueElement* element, uint state)
//{
//  //volatile global int* pstate = ((volatile global int*)element) + 3;
//  //uint old = atomic_xchg(pstate, state);
//  atomic_xchg(&element->state, state);
//  //check: old + 1 % (QueueElementReading+1) == state
//}
//uint getQueueState(volatile global QueueElement* element)
//{
//  return element->state;
//  //return element->w;
//}
//void waitForQueueState(volatile global QueueElement* element, uint expected)
//{
//  while(getQueueState(element) != expected);
//}
//
//int3 getBlockId(volatile global QueueElement* element)
//{
//  return (int3)(element->blockx, element->blocky, element->blockz);
//  //return (*element).xyz;
//}
//
//void setBlockId(volatile global QueueElement* element, int3 block)
//{
//  element->blockx = block.x;
//  element->blocky = block.y;
//  element->blockz = block.z;
//  //(*element).xyz = block;
//}
//
//struct _QueueGlobal
//{
//  uint front;
//  uint back;
//  int filllevel;
//  uint activeBlocks;
//  uint processedBlocks;
//  uint sortingBarrier;
//  int debug;
//  uint mincost;
//};
//typedef struct _QueueGlobal QueueGlobal;
//
//#define linAccess3D(id, dim) \
//  ((id).x + ( (id).y + (id).z  * (dim.y) )* (dim).x)
//
//float atomic_min_float(volatile global float* p, float val)
//{
//  return as_float(atomic_min((volatile global uint*)(p),as_uint(val)));
//}
//float atomic_min_float_local(volatile local float* p, float val)
//{
//  return as_float(atomic_min((volatile local uint*)(p), as_uint(val)));
//}
//
//void setPriorityEntry(global float* p)
//{
//#if NOATOMICFLOAT
//  *(global uint*)(p) = MAGICPRIORITY;
//#else
//  *p = MAGICPRIORITYF;
//#endif
//}
//void removePriorityEntry(volatile global float* p)
//{
//#if NOATOMICFLOAT
//  atomic_xchg((volatile global uint*)(p), MAGICPRIORITY);
//#else
//  atomic_xchg(p, MAGICPRIORITYF);
//#endif
//}
//bool updatePriority(volatile global float* p, float val)
//{
//#if NOATOMICFLOAT
//  uint nval = val * PRECISSIONMULTIPLIER;
//  uint v = atomic_min((volatile global uint*)(p),nval);
//  return v != MAGICPRIORITY;
//#else
//  float old_priority = atomic_min_float(p, val);
//  return old_priority != MAGICPRIORITYF;
//#endif
//}
//
//
//void Enqueue(volatile global QueueGlobal* queueInfo, 
//             volatile global QueueElement* queue,
//             volatile global float* queuePriority,
//             int3 block,
//             float priority,
//             int2 blocks,
//             uint queueSize)
//{
//  //check if element is already in tehe Queue:
//  
//  volatile global float* myQueuePriority = queuePriority + linAccess3D(block, blocks);
//
//  //update priority:
//  bool hadPriority = updatePriority(myQueuePriority, priority);
//  
//  //if there is a priority, it is in the queue
//  if(hadPriority)
//    return;
//  else
//  {
//    //add to queue
//
//    //inc size 
//    int filllevel = atomic_inc(&queueInfo->filllevel);
//    
//    //make sure there is enough space?
//    //assert(filllevel < queueSize)
//
//    //insert a new element
//    uint pos = atomic_inc(&queueInfo->back)%queueSize;
//
//    //make sure it is free?
//    waitForQueueState(queue + pos, QueueElementEmpty);
//    //queue[pos] == QueueElementEmpty
//
//    //insert the data
//    setBlockId(queue + pos, block);
//    mem_fence(CLK_GLOBAL_MEM_FENCE);
//    
//    //increase the number of active Blocks
//    atomic_inc(&queueInfo->activeBlocks);
//
//    //activate the queue element
//    setQueueState(queue + pos,  QueueElementReady);
//
//  }
//}
//
//bool Dequeue(volatile global QueueGlobal* queueInfo, 
//             volatile global QueueElement* queue,
//             volatile global float* queuePriority,
//             int3* element,
//             int2 blocks,
//             uint queueSize)
//{
//  //is there something to get?
//  if(atomic_dec(&queueInfo->filllevel) <= 0)
//  {
//    atomic_inc(&queueInfo->filllevel);
//    return false;
//  }
//
//  //pop the front of the queue
//  uint pos = atomic_inc(&queueInfo->front)%queueSize;
//
//  //check for barrier
//  uint barrierPos = queueInfo->sortingBarrier;
//  uint back = queueInfo->back%queueSize;
//  while(barrierPos <= pos && barrierPos > back)
//  {
//    barrierPos = queueInfo->sortingBarrier;
//    queueInfo->debug += 100000;
//  }
//  
//  //wait for the element to be written/change to reading
//  checkAndSetQueueState(queue + pos, QueueElementReading, QueueElementReady);
//  
//  //read
//  int3 blockId = getBlockId(queue + pos);
//  read_mem_fence(CLK_GLOBAL_MEM_FENCE);
//  
//  //remove priority (mark as not in queue)
//  volatile global float* myQueuePriority = queuePriority + linAccess3D(blockId, blocks);
//  removePriorityEntry(myQueuePriority);
//
//  //free queue element
//  setQueueState(queue + pos, QueueElementRead);
//  *element = blockId;
//  return true;
//}
////
//
//
//float getPenalty(read_only image2d_t costmap, int2 pos)
//{
//  const sampler_t sampler = CLK_FILTER_NEAREST
//                          | CLK_NORMALIZED_COORDS_FALSE
//                          | CLK_ADDRESS_CLAMP_TO_EDGE;
//
//  return read_imagef(costmap, sampler, pos).x;
//}
//
//__kernel void clearQueueLink(global float* queuePriority)
//{
//  setPriorityEntry(queuePriority + get_global_id(0));
//}
//__kernel void clearQueue(global QueueElement* queue)
//{
//  setQueueState(queue + get_global_id(0),  QueueElementEmpty);
//}
//
////it might be better to do that via opengl or something (arbitrary node shapes etc)
//__kernel void prepareRouting(read_only image2d_t costmap,
//                           global float* routecost,
//                           global QueueElement* queue,
//                           global float* queuePriority,
//                           global QueueGlobal* queueInfo,
//                           uint queueSize,
//                           global uint4* startingPoints,
//                           const uint numStartingPoints,
//                           const int2 dim,
//                           const int2 numBlocks)
//{
//  __local bool hasStartingPoint;
//  
//
//  int3 id = {get_global_id(0), get_global_id(1), get_global_id(2)};
//
//  hasStartingPoint = false;
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//  if(id.x < dim.x && id.y < dim.y)
//  {
//    float cost = 0.01f*MAXFLOAT;
//    if(startingPoints[id.z].x <= id.x && id.x <= startingPoints[id.z].z &&
//        startingPoints[id.z].y <= id.y && id.y <= startingPoints[id.z].w)
//    {
//        cost = 0;
//        hasStartingPoint = true;
//    }
//    //prepare data
//    routecost[dim.x*id.y+id.x + dim.x*dim.y*id.z] = cost;
//  }
//  barrier(CLK_LOCAL_MEM_FENCE);
//  if(hasStartingPoint && get_local_id(0) == 0 && get_local_id(1) == 0)
//  {
//    //insert into queue
//    int3 block = (int3)(get_group_id(0), get_group_id(1), id.z);
//    Enqueue(queueInfo, queue, queuePriority, block, 0, numBlocks, queueSize);
//  }
//}
//
//
//local float* accessLocalCost(local float* l_costs, int2 id)
//{
//  return l_costs + (id.x+1 + (id.y+1)*(get_local_size(0)+2));
//}
//
//void copyInLocalData(global float* routecost,
//           int3 blockid,
//           local float* l_costs,
//           int2 dim)
//{
//  blockid.x *= get_local_size(0);
//  blockid.y *= get_local_size(1);
//
//  //copy cost to local
//  int local_linid = get_local_id(1)*get_local_size(0) + get_local_id(0);
//  for(int i = local_linid; i < (get_local_size(0)+2)*(get_local_size(1)+2); i += get_local_size(0)*get_local_size(1))
//  {
//    int2 global_pos = (int2)(blockid.x, blockid.y) + (int2)(i%(get_local_size(0)+2),i/(get_local_size(0)+2)) - (int2)(1,1);
//    
//    float incost = 0.01f*MAXFLOAT;
//    if(global_pos.x > 0 && global_pos.x < dim.x &&
//       global_pos.y > 0 && global_pos.y < dim.y)
//      incost = routecost[dim.x*global_pos.y + global_pos.x + dim.x*dim.y*blockid.z]; 
//    l_costs[i] = incost;
//  }
//}
//
//void copyOutLocalData(global float* routecost,
//           int3 blockid,
//           local float* l_costs,
//           int2 dim)
//{
//  //copy cost to global
//  int2 global_pos = (int2)(get_local_size(0)*blockid.x + get_local_id(0), get_local_size(1)*blockid.y + get_local_id(1));
//  if(global_pos.x < dim.x && global_pos.y < dim.y)
//    routecost[dim.x*global_pos.y + global_pos.x + dim.x*dim.y*blockid.z] = *accessLocalCost(l_costs, (int2)(get_local_id(0), get_local_id(1)));
//}
//
//
//bool route_data_available(read_only image2d_t costmap,
//           global float* routecost,
//           int3 blockid,
//           local float* l_costs,
//           int2 dim)
//{
//
//  //route as long as there is change
//  local bool change;
//  change = true;
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//  int2 localid = (int2)(get_local_id(0),get_local_id(1));
//  int2 globalid = (int2)(blockid.x*get_local_size(0)+get_local_id(0),blockid.y*get_local_size(1)+get_local_id(1));
//
//  //iterate over it
//  float myPenalty = getPenalty(costmap, globalid);
//  float local * myval = accessLocalCost(l_costs, localid);
//  int counter = -1;
//  while(change)
//  {
//    change = false;
//    barrier(CLK_LOCAL_MEM_FENCE);
//
//    float lastmyval = *myval;
//    
//    int2 offset;
//    for(offset.y = -1; offset.y  <= 1; ++offset.y)
//      for(offset.x = -1; offset.x <= 1; ++offset.x)
//      {
//        //TODO d is either 1 or M_SQRT2 - maybe it is faster to substitute
//        //float d = native_sqrt((float)(offset.x*offset.x+offset.y*offset.y));
//        int d2 = offset.x*offset.x+offset.y*offset.y;
//        float d = (d2-1)*M_SQRT2 + (2-d2);
//        float penalty = 0.5f*(myPenalty + getPenalty(costmap, globalid + offset));
//        float c_other = *accessLocalCost(l_costs, localid + offset);
//        //TODO use custom params here
//        *myval = min(*myval, c_other + 0.01f*d + d*penalty);
//      }
//
//    if(*myval != lastmyval)
//      change = true;    
//    
//    barrier(CLK_LOCAL_MEM_FENCE);
//    ++counter;
//  }
//
//  copyOutLocalData(routecost, blockid, l_costs, dim);
//  return counter > 0;
//}
//
//bool route(read_only image2d_t costmap,
//           global float* routecost,
//           int3 blockid,
//           local float* l_costs,
//           int2 dim)
//{
//  copyInLocalData(routecost, blockid, l_costs, dim);
//  return route_data_available(costmap, routecost, blockid, l_costs, dim);
//}
//
//void minBorderCosts(local float* l_costs,
//                    local float borderCosts[4])
//{
//#if NOATOMICFLOAT
//  local uint* l_cost = (local uint*)(borderCosts);
//  if(get_local_id(0) < 4)
//    l_cost[get_local_id(0)] = MAGICPRIORITY;
//  barrier(CLK_LOCAL_MEM_FENCE);
//  float myCost = *accessLocalCost( l_costs, (int2)(get_local_id(0), get_local_id(1)) );
//  uint myIntCost =  myCost * PRECISSIONMULTIPLIER;
//  if(get_local_id(0) == 0)
//    atomic_min(&l_cost[0], myIntCost);
//  else if(get_local_id(0) == get_local_size(0) - 1)
//    atomic_min(&l_cost[2], myIntCost);
//  if(get_local_id(1) == 0)
//    atomic_min(&l_cost[1], myIntCost);
//  else if(get_local_id(1) == get_local_size(1) - 1)
//    atomic_min(&l_cost[3], myIntCost);
//  barrier(CLK_LOCAL_MEM_FENCE);
//  if(get_local_id(0) < 4)
//    borderCosts[get_local_id(0)] = ((float)l_cost[get_local_id(0)])/PRECISSIONMULTIPLIER;
//#else
//  if(get_local_id(0) < 4)
//    borderCosts[get_local_id(0)] = MAXFLOAT;
//  barrier(CLK_LOCAL_MEM_FENCE);
//  float myCost = *accessLocalCost( l_costs, (int2)(get_local_id(0), get_local_id(1)) );
//  if(get_local_id(0) == 0)
//    atomic_min_float_local(&borderCosts[0], myCost);
//  else if(get_local_id(0) == get_local_size(0) - 1)
//    atomic_min_float_local(&borderCosts[2], myCost);
//  if(get_local_id(1) == 0)
//    atomic_min_float_local(&borderCosts[1], myCost);
//  else if(get_local_id(1) == get_local_size(1) - 1)
//    atomic_min_float_local(&borderCosts[3], myCost);
//#endif
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//}
//
//void routing_worker(read_only image2d_t costmap,
//                      global float* routecost,
//                      volatile global QueueElement* queue,
//                      volatile global float* queuePriority,
//                      volatile global QueueGlobal* queueInfo,
//                      uint queueSize,
//                      const int2 dim,
//                      const int2 numBlocks,
//                      const uint processedBlocksThreshold, 
//                      local float* l_costs)
//{
//  while(queueInfo->activeBlocks > 0 && queueInfo->processedBlocks < processedBlocksThreshold)
//  {
//    local int3 blockId;
//    if(get_local_id(0) == 0 && get_local_id(1) == 0)
//    {
//      int3 element;
//      if(Dequeue(queueInfo, queue, queuePriority, &element, numBlocks, queueSize))
//        blockId = element;
//      else
//        blockId.x = -1;
//    }
//    barrier(CLK_LOCAL_MEM_FENCE);
//    
//    if(blockId.x > -1)
//    {
//      //copy in
//      copyInLocalData(routecost, blockId, l_costs, dim);
//      //get current state
//      local float initborderCosts[4];
//      minBorderCosts(l_costs, initborderCosts);
//
//      //process it
//      bool change = route_data_available(costmap, routecost, blockId, l_costs, dim);
//
//      //add surrounding ones
//      if(change)
//      {
//        local float borderCosts[4];
//        minBorderCosts(l_costs, borderCosts);
//        if(get_local_id(0) == 0 && get_local_id(1) == 0)
//        {
//          int3 block = blockId;
//          if(blockId.x - 1 >= 0 && initborderCosts[0] > borderCosts[0])
//          {
//            block.x = blockId.x - 1;
//            Enqueue(queueInfo, queue, queuePriority, block, borderCosts[0], numBlocks, queueSize);
//            block.x = blockId.x;
//          }
//          if(blockId.x + 1 < numBlocks.x && initborderCosts[2] > borderCosts[2])
//          {
//            block.x = blockId.x + 1;
//            Enqueue(queueInfo, queue, queuePriority, block, borderCosts[2], numBlocks, queueSize);
//            block.x = blockId.x;
//          }
//          if(blockId.y - 1 >= 0 && initborderCosts[1] > borderCosts[1])
//          {
//            block.y = blockId.y - 1;
//            Enqueue(queueInfo, queue, queuePriority, block, borderCosts[1], numBlocks, queueSize);
//          }
//          if(blockId.y + 1 < numBlocks.y && initborderCosts[3] > borderCosts[3])
//          {
//            block.y = blockId.y + 1;
//            Enqueue(queueInfo, queue, queuePriority, block, borderCosts[3], numBlocks, queueSize);
//          }
//        }
//      }
//      //else if(get_local_id(0) == 0 && get_local_id(1) == 0)
//      //  atomic_inc(&queueInfo->debug);
//
//      //update counter
//      if(get_local_id(0) == 0 && get_local_id(1) == 0)
//      {
//        atomic_dec(&queueInfo->activeBlocks);
//        atomic_inc(&queueInfo->processedBlocks);
//      }
//      
//    }
//    barrier(CLK_LOCAL_MEM_FENCE);
//  }
//}
//
//
//uint wrappedAroundQueueId(uint id, uint queueSize)
//{
//  return min(id, id + queueSize);
//}
//void sortQueue(volatile global QueueElement* queue,
//               volatile global QueueGlobal* queueInfo,
//               volatile global float* queuePriorities,
//               uint queueSize,
//               const int2 numBlocks,
//               const uint processedBlocksThreshold, 
//               local float* l_priorites,
//               local uint* l_ids)
//{
//  local bool ok;
//  uint nextsort = queueSize + 1;
//  uint linid = get_local_id(1) * get_local_size(0) + get_local_id(0);
//  uint threads = get_local_size(0)*get_local_size(1);
//  uint sortMargin =  4*threads;
//
//  //prepare local ids
//  l_ids[linid] = linid;
//  l_ids[threads + linid] = threads + linid;
//  
//  while(queueInfo->activeBlocks > 0 && queueInfo->processedBlocks < processedBlocksThreshold)
//  {
//    ok = true;
//    barrier(CLK_LOCAL_MEM_FENCE); 
//
//    //restart at back?
//    if(nextsort > queueSize)
//      nextsort = queueInfo->back % queueSize; //the back might just be adding
//
//
//    //is the distance bigger than the margin? (ring buffer)
//    uint margin = wrappedAroundQueueId(nextsort - (queueInfo->front%queueSize), queueSize);
//    if(margin < sortMargin)
//    {
//      //margin is too small, dont even bother
//      nextsort = queueSize + 1;
//      continue;
//    }
//
//    //copy the data in
//    uint in_out_id0 = wrappedAroundQueueId(nextsort - 2*threads + linid, queueSize);
//    //make sure it is ready
//    int err_counter = 100;
//    while(getQueueState(queue + in_out_id0) != QueueElementReady && --err_counter);
//    if(!err_counter) ok = false;
//    int3 blockId0 = getBlockId(queue + in_out_id0);
//    l_priorites[linid] = queuePriorities[linAccess3D(blockId0, numBlocks)];
//
//    uint in_out_id1 = wrappedAroundQueueId(nextsort - threads + linid, queueSize);
//    //make sure it is ready
//    err_counter = 100;
//    while(getQueueState(queue + in_out_id1) != QueueElementReady && --err_counter);
//    if(!err_counter) ok = false;
//    int3 blockId1 = getBlockId(queue + in_out_id1);
//    l_priorites[threads +  linid] = queuePriorities[linAccess3D(blockId1, numBlocks)];
//
//    barrier(CLK_LOCAL_MEM_FENCE); 
//
//    if(!ok) //loading did not work
//      continue;
//
//    //sort
//    bitonicSort(linid,l_priorites,l_ids,2*get_local_size(0),true);
//
//    uint target0 = l_ids[linid];
//    uint target1 = l_ids[threads + linid];
//    
//    //copy blockids around
//    #define copyBlockIds(COMPONENT) \
//      barrier(CLK_LOCAL_MEM_FENCE); \
//      l_ids[target0] = blockId0.COMPONENT; \
//      l_ids[target1] = blockId1.COMPONENT; \
//      barrier(CLK_LOCAL_MEM_FENCE); \
//      blockId0.COMPONENT = l_ids[linid]; \
//      blockId1.COMPONENT = l_ids[threads + linid];
//
//    copyBlockIds(x);
//    copyBlockIds(y);
//    copyBlockIds(z);
//
//    #undef copyBlockIds
//
//    //check if we can write it back
//    margin = wrappedAroundQueueId(nextsort - (queueInfo->front%queueSize), queueSize);
//    if(margin > 3*threads)
//    {
//      //set the barrier
//      queueInfo->sortingBarrier =  wrappedAroundQueueId(nextsort - 2*threads, queueSize);
//      write_mem_fence(CLK_GLOBAL_MEM_FENCE);
//
//      //check if it is still ok
//      margin = wrappedAroundQueueId(nextsort - (queueInfo->front%queueSize), queueSize);
//      if(margin > 2*threads)
//      {
//        //write back
//        setBlockId(queue + in_out_id0, blockId0);
//        setBlockId(queue + in_out_id1, blockId1);
//      }
//      //remove barrier
//      queueInfo->sortingBarrier = queueSize + 1;
//      write_mem_fence(CLK_GLOBAL_MEM_FENCE);
//      queueInfo->debug++;
//    }
//    else
//    {
//      //increase sort margin
//      sortMargin += threads;
//      //restart sort at the beginning
//      nextsort = queueSize + 1;
//      queueInfo->debug+= 1000;
//    }
//
//    //reset local ids
//    l_ids[linid] = linid;
//    l_ids[threads + linid] = threads + linid;
//  }
//}
//
//__kernel void routing(read_only image2d_t costmap,
//                      global float* routecost,
//                      volatile global QueueElement* queue,
//                      volatile global float* queuePriority,
//                      volatile global QueueGlobal* queueInfo,
//                      uint queueSize,
//                      const int2 dim,
//                      const int2 numBlocks,
//                      const uint processedBlocksThreshold, 
//                      local float* l_data)
//{
//  //if(get_group_id(0) + get_group_id(1) + get_group_id(2) == 0)
//  ////sorter
//  //  sortQueue(queue, 
//  //            queueInfo,
//  //            queuePriority,
//  //            queueSize,
//  //            numBlocks,
//  //            processedBlocksThreshold,
//  //            l_data,
//  //            (local uint*)( l_data+2*get_local_size(0)*get_local_size(1) ) );
//  //else
//    //worker
//     routing_worker(costmap, 
//                    routecost,
//                    queue,
//                    queuePriority,
//                    queueInfo,
//                    queueSize,
//                    dim,
//                    numBlocks,
//                    processedBlocksThreshold, 
//                    l_data);
//}
//
//__kernel void getMinimum(global const float* routecost,
//                         volatile global QueueGlobal* queueInfo,
//                         volatile global ushort2* coordinates,
//                         const int2 dim,
//                         int targets)
//{
//  float cost = 0;
//  for(int i = 0; i < targets; ++i)
//    cost += routecost[dim.x*get_global_id(1) + get_global_id(0) + dim.x*dim.y*i];
//
//  __local uint localmin;
//  __local uint2 coords;
//  localmin = as_uint(MAXFLOAT);
//
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//  atomic_min(&localmin, as_uint(cost));
//
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//  if(localmin == as_uint(cost))
//    coords = (uint2)(get_global_id(0), get_global_id(1));
//
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//  if( (get_local_id(0) + get_local_id(1)) == 0 )
//  {
//    uint minv = as_uint(localmin);
//    //compress it
//    minv = (minv << 1) & 0xFFFFFF00;
//    //combine with block id
//    uint groupv = (get_group_id(0) + get_group_id(1)*get_num_groups(0)) & 0xFF;
//    uint combined = minv | groupv;
//    uint old = atomic_min(&queueInfo->mincost, combined);
//    if(old > combined)
//    {
//      //write coords to array entry
//      coordinates[groupv] = (ushort2)(coords.x,coords.y);
//    }
//  }
//}
//
//uint borderId(int2 lid, int2 lsize)
//{
//  uint id = lid.x;
//  if(lid.x==lsize.x-1)
//    id += lid.y;
//  else if(lid.y==lsize.y-1)
//    id = lsize.x*2 + lsize.y - 3 - lid.x;
//  else if(lid.x == 0 && lid.y > 0)
//    id = 2*(lsize.x + lsize.y - 2) - lid.y;
//  return id;
//}
//
//__kernel void calcInOut(global const float* routecost,
//                        global uint* blockroutes,
//                        local int* l_cost,
//                        local int* l_origin,
//                        const int2 start,
//                        const int2 dim)
//{
//  int3 coord = (int3)(get_global_id(0), get_global_id(1), get_global_id(2));
//  float mincost = MAXFLOAT;
//  if(coord.x >= 0 && coord.x < dim.x && 
//     coord.y >= 0 && coord.y < dim.y)
//    mincost =  routecost[coord.x + dim.x*coord.y + dim.x*dim.y*coord.z];
//
//  int2 dir = (int2)(0,0);
//  for(int y = -1; y <= 1; ++y)
//    for(int x = -1; x <= 1; ++x)
//    {
//      coord = (int3)(get_global_id(0)+x, get_global_id(1)+y, get_global_id(2));
//      if(coord.x >= 0 && coord.x < dim.x && 
//         coord.y >= 0 && coord.y < dim.y)
//      {
//        float tcost =  routecost[coord.x + dim.x*coord.y + dim.x*dim.y*coord.z];
//        if(tcost < mincost)
//        {
//          mincost = tcost;
//          dir = (int2)(x,y);
//        }
//      }
//    }
//  //store dir and cost
//  int2 lid = (int2)(get_local_id(0), get_local_id(1));
//  int2 fromid = lid + dir;
//  
//  bool outside = fromid.x < 0 ||
//                fromid.y < 0 ||
//                fromid.x >= get_local_size(0) ||
//                fromid.y >= get_local_size(1);
//
//  int cost = (dir.x == 0 && dir.y == 0) ? 0 : 
//             (outside ? 1 : (2*get_local_size(0)*get_local_size(1)));
//
//  int origin = (dir.x == 0 && dir.y == 0) ? ( ( (1 + lid.y) << 8 ) | (lid.x + 1)) :
//               (outside ? ( ( (1 + fromid.y) << 8 ) | (1 + fromid.x ) ) : -1);
//
//  
//  int loffset = lid.x + lid.y*(int)get_local_size(0);
//  int lfrom = fromid.x + fromid.y*(int)get_local_size(0);
//  l_cost[loffset] = cost;
//  l_origin[loffset] = origin;
//  
//  __local bool changed;
//  uint count = 0;
//  do
//  {
//    changed = false;
//    barrier(CLK_LOCAL_MEM_FENCE);
//    if(origin == -1)
//    {
//      if(l_cost[loffset] != l_cost[lfrom]+1)
//      {
//        l_cost[loffset] = l_cost[lfrom]+1;
//        l_origin[loffset] = l_origin[lfrom];
//        changed = true;
//      }
//    }      
//    barrier(CLK_LOCAL_MEM_FENCE);
//  }
//  while(changed);
//
//
//  bool border =  get_local_id(0) == 0 ||
//                 get_local_id(1) == 0 ||
//                 get_local_id(0) == get_local_size(0)-1 ||
//                 get_local_id(1) == get_local_size(1)-1;
//  
//  uint info = (l_cost[loffset] << 16) | (l_origin[loffset] &0xFFFF);
//
//  if(border)
//  {
//    //store the info
//    uint localoffset = borderId((int2)(get_local_id(0),get_local_id(1)), (int2)(get_local_size(0),get_local_size(1)));    
//    uint groupid = get_group_id(0)+get_group_id(1)*get_num_groups(0) + get_group_id(2)*get_num_groups(0)*get_num_groups(1);
//    uint elements_per_group = 2*(get_local_size(0) + get_local_size(1) - 2);
//    //blockroutes[0] = info;
//    blockroutes[ get_global_size(2) + groupid * elements_per_group + localoffset] =  info;
//  }
//
//  if( start.x == get_global_id(0) && start.y == get_global_id(1) )
//  {
//    //this is the starting point, we also need to save this info
//    blockroutes[ get_global_id(2) ] =  info;
//  }
//  
//}
//
//
//uint blockInOutCost(uint data)
//{
//  return data >> 16;
//}
//int2 blockInOutOrigin(uint data)
//{
//  int x = data & 0xFF;
//  int y = (data >> 8) & 0xFF;
//  return (int2)(x-1,y-1);
//}
//uint pack(int2 d)
//{
//  return (d.x << 16) | d.y;
//}
//int2 unpack(uint d)
//{
//  return (int2)(d >> 16, d & 0xFFFF); 
//}
//
//__kernel void calcInterBlockRoute(global const uint* blockroutes,
//                                  global uint2* interblockroutes,
//                                  const int2 start,
//                                  const int2 blocks,
//                                  const int2 blocksize)
//{
//  uint target = get_global_id(0);
//  uint startinfo = blockroutes[target];
//  uint elements_per_group = 2*(blocksize.x + blocksize.y - 2);
//  uint inoffset = get_global_size(0) + target*blocks.x*blocks.y*elements_per_group;
//  uint outoffset = blocks.x*blocks.y*target + get_global_size(0);
//  uint blockcount = 0;
//
//  uint newcost = blockInOutCost(startinfo);
//  int2 localorigin = blockInOutOrigin(startinfo);
//  interblockroutes[outoffset++] = (uint2)(0,pack(start));
//
//  int2 block = (int2)(start.x/blocksize.x, start.y/blocksize.y);
//  uint sumcost = 0;
//  int2 blockdiff = (int2)(0,0);
//
//  do
//  {
//    sumcost += newcost;
//    int2 currentpos = block*blocksize + localorigin;
//    blockdiff = (int2)(localorigin.x < 0 ? -1 : (localorigin.x >= blocksize.x ? 1 : 0),
//                       localorigin.y < 0 ? -1 : (localorigin.y >= blocksize.y ? 1 : 0));
//    block.x += blockdiff.x;
//    block.y += blockdiff.y;
//    int2 localpos = currentpos - block*blocksize;
//
//    int localoffset = borderId(localpos, blocksize);
//    interblockroutes[outoffset++] = (uint2)(sumcost,pack(currentpos));
//
//    uint currentinfo = blockroutes[inoffset + (block.y*blocks.x + block.x)*elements_per_group + localoffset];
//    newcost = blockInOutCost(currentinfo);
//    localorigin = blockInOutOrigin(currentinfo);
//    ++blockcount;
//
//  } while(blockdiff.x != 0 || blockdiff.y != 0);
//
//  interblockroutes[target].x = blockcount;
//  interblockroutes[target].y = sumcost+1;
//}
//
//__kernel void constructRoute(global const float* routecost,
//                             global const uint2* interblockroutes,
//                             global int2* route,
//                             const int maxblocks_pertarget,
//                             const int maxpoints_pertarget,
//                             const int numtargets,
//                             const int2 dim,
//                             const int2 blocks)
//{
//  uint target = get_global_id(0)/maxblocks_pertarget;
//  uint element = get_global_id(0)%maxblocks_pertarget;
//  if(element >= interblockroutes[target].x)
//    return;
//  
//  int offset = maxpoints_pertarget*target + interblockroutes[blocks.x*blocks.y*target + numtargets + element].x;
//  int endoffset = maxpoints_pertarget*target + interblockroutes[blocks.x*blocks.y*target + numtargets + element + 1].x;
//  int2 pos = unpack( interblockroutes[blocks.x*blocks.y*target + numtargets + element].y );
//
//  route[offset++] = pos;
//  //follow the route
//  while(offset <= endoffset)
//  {
//    float mincost = MAXFLOAT;
//    int2 next = pos;
//    for(int y = -1; y <= 1; ++y)
//      for(int x = -1; x <= 1; ++x)
//      {
//        int2 coord = (int2)(pos.x+x, pos.y+y);
//        if(coord.x >= 0 && coord.x < dim.x && 
//           coord.y >= 0 && coord.y < dim.y)
//        {
//          float tcost =  routecost[coord.x + dim.x*coord.y + dim.x*dim.y*target];
//          if(mincost > tcost)
//          {
//            mincost = tcost;
//            next = coord;
//          }
//        }
//      }
//    pos = next;
//    route[offset++] = pos;
//  }
//}
//
//
//__kernel void route_no_queue(read_only image2d_t costmap,
//                      global float* routecost,
//                      const int2 dim,
//                      local float* l_costs)
//{
//  int3 blockid = {get_group_id(0), get_group_id(1), get_global_id(2)};
//  route(costmap, routecost, blockid, l_costs, dim);
//  return;
//}
//
//#if 0
//  int3 id = {get_global_id(0), get_global_id(1), get_global_id(2)};
//
//  //copy cost to local
//  int local_linid = get_local_id(1)*get_local_size(0) + get_local_id(0);
//  for(int i = local_linid; i < (get_local_size(0)+2)*(get_local_size(1)+2); i += get_local_size(0)*get_local_size(1))
//  {
//    int2 global_pos = (int2)(get_group_id(0)*get_local_size(0),get_group_id(1)*get_local_size(1));
//    global_pos += (int2)(i%(get_local_size(0)+2),i/(get_local_size(0)+2)) - (int2)(1,1);
//    
//    float incost = 0.01f*MAXFLOAT;
//    if(global_pos.x > 0 && global_pos.x < dim.x &&
//       global_pos.y > 0 && global_pos.y < dim.y)
//      incost = routecost[dim.x*global_pos.y + global_pos.x + dim.x*dim.y*id.z]; 
//    l_costs[i] = incost;
//  }
//
//  local bool change;
//  if(local_linid == 0)
//    change = true;
//  barrier(CLK_LOCAL_MEM_FENCE);
//
//  int2 localid = (int2)(get_local_id(0),get_local_id(1));
//  int2 globalid = (int2)(get_global_id(0),get_global_id(1));
//
//  //iterate over it
//  float myPenalty = getPenalty(costmap, globalid);
//  float local * myval = accessLocalCost(l_costs, localid);
//  while(change)
//  {
//    if(local_linid == 0)
//      change = false;
//    barrier(CLK_LOCAL_MEM_FENCE);
//
//    float lastmyval = *myval;
//    
//    int2 offset;
//    for(offset.y = -1; offset.y  <= 1; ++offset.y)
//      for(offset.x = -1; offset.x <= 1; ++offset.x)
//      {
//
//        float d = native_sqrt((float)(offset.x*offset.x+offset.y*offset.y));
//        float penalty = 0.5f*(myPenalty + getPenalty(costmap, globalid + offset));
//        float c_other = *accessLocalCost(l_costs, localid + offset);
//        *myval = min(*myval, c_other + 0.01f*d + d*penalty);
//      }
//
//    if(*myval != lastmyval)
//      change = true;    
//
//    barrier(CLK_LOCAL_MEM_FENCE);
//  }
//
//  //write back
//  routecost[dim.x*globalid.y + globalid.x + dim.x*dim.y*id.z] = *myval;
//}
//#endif
//
//
//#if 0
//
///*
// The values stored in the resulting map contain the cost the reach each node and
// which is its predecessor on the path there. It contains 32 Bits for each node
// where the first two bits represent the direction of the predecessor and the
// remaining bits form the actual cost of reaching the node. 
// */
//
//constant uint MASK_VALUE        = 0x3fffffff;
//constant uint MASK_DIRECTION    = 0xc0000000;
//constant uint VALUE_NOT_VISITED = 0xffffffff;
//
////------------------------------------------------------------------------------
//// Heap
////------------------------------------------------------------------------------
//typedef struct
//{
//  global uint *values; //!< The values to be sorted
//  global uint *heap;   //!< Sorted indices (by values)
//  uint size;           //!< Current heap size
//  int2 dim;            //!< Dimensions of values
//} data_t;
//
//#define _value(index) (d->values[ d->heap[index] ] & MASK_VALUE)
//#define _parent(index) (index / 2)
//#define _child_left(index) (2 * index + 1)
//#define _child_right(index) (2 * index + 2)
//
///**
// * @param index The index of the node to insert
// */
//void heap_insert( data_t *d,
//                  const uint index )
//{
//  uint cur = d->size++;
//  uint val = _value(cur);
//  uint parent = _parent(cur);
//
//  // move up
//  while( cur && val < _value(parent) )
//  {
//    d->heap[ cur ] = d->heap[ parent ];
//
//    cur = parent;
//    parent = _parent(cur);
//  }
//  
//  d->heap[ cur ] = index;
//}
//
///**
// *
// * @return Index of the minimum node
// */
//uint heap_removeMin( data_t *d )
//{
//  // Remove first element
//  uint cur = 0,
//       front = d->heap[ cur ],
//       child = _child_left(cur);
//  
//  // Propagate hole down the tree
//  while( child < d->size )
//  {
//    // Change to right child if it exists and is smaller than the left one
//    if( child + 1 < d->size && _value(child + 1) < _value(child) )
//      child += 1;
//
//    d->heap[ cur ] = d->heap[ child ];
//    cur = child;
//    child = _child_left(cur);
//  }
//
//  // Put last element into hole...
//  uint index = d->heap[ --d->size ];
//  uint val = _value(index);
//  uint parent = _parent(cur);
//
//  // ...and let it bubble up
//  while( cur && val < _value(parent) )
//  {
//    d->heap[ cur ] = d->heap[ parent ];
//
//    cur = parent;
//    parent = _parent(cur);
//  }
//  
//  d->heap[ cur ] = index;
//  return front;
//}
//
////------------------------------------------------------------------------------
//// Dijkstra
////------------------------------------------------------------------------------
//float getPenalty(read_only image2d_t costmap, int x, int y)
//{
//  const sampler_t sampler = CLK_FILTER_NEAREST
//                          | CLK_NORMALIZED_COORDS_FALSE
//                          | CLK_ADDRESS_CLAMP_TO_EDGE;
//
//  return read_imagef(costmap, sampler, (int2)(x, y)).x;
//}
//
////------------------------------------------------------------------------------
//// Kernel
////------------------------------------------------------------------------------
//
//#define makeIndex(pos, dim) (pos.y * dim.x + pos.x)
//
//__kernel void test_kernel( read_only image2d_t costmap,
//                           global uint* nodes,
//                           global uint* queue,
//                           const int2 dim,
//                           const int2 start,
//                           const int2 target )
//{
//  data_t data = {
//    .values = nodes,
//    .heap = queue,
//    .size = 0,
//    .dim = dim
//  };
//  
//  const float link_width = 4,
//              alpha_L = 0.01,
//              alpha_P = 2.99,
//              // simplify and scale the cost calculation a bit...
//              factor = 100 * (0.5/alpha_L * alpha_P * link_width + 1);
//
//
//  // Initialize all to "Not visited" and startnode to zero
//  for(uint i = 0; i < dim.x * dim.y; ++i)
//    nodes[i] = VALUE_NOT_VISITED;
//
//  uint start_index = makeIndex(start, dim);
//  nodes[ start_index ] = 0;
//  heap_insert(&data, start_index);
//  int count = 10;
//  uint max_size = 0;
//  // Start with Dijkstra    
//  do
//  {
//    max_size = max(data.size, max_size);
//    uint index = heap_removeMin(&data);
//    int px = index % dim.x, // DON'T use int2 -> don't know why but % doesn't work...
//        py = index / dim.x;
//
//    // Update all 8 neighbours
//    for( int x = max(px - 1, 0); x <= min(px + 1, dim.x); ++x )
//      for( int y = max(py - 1, 0); y <= min(py + 1, dim.y); ++y )
//      {
//        if( px == x && py == y )
//          continue;
//
//        int index_n = makeIndex((int2)(x, y), dim);
//
//        if( nodes[index_n] == VALUE_NOT_VISITED )
//          heap_insert(&data, index_n);
//
//        float len = (x == px || y == py) ? 1 : 1.4f;
//        float cost = factor
//                   * len
//                   * (getPenalty(costmap, x, y) + getPenalty(costmap, px, py));
//
//        int total_cost = nodes[index] + (int)cost;
//        
//        // update/relax
//        if( total_cost < nodes[index_n] )
//        {
//          nodes[index_n] = total_cost;
//        }
//      }
//  } while( data.size && count-- );
//  
//  nodes[ 0 ] = max_size;
//}
//
//#endif