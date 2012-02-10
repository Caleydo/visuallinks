
//bitonic sorting
//see http://www.iti.fh-flensburg.de/lang/algorithmen/sortieren/bitonic/bitonicen.htm


#ifndef KEY_TYPE
#define KEY_TYPE uint
#endif

#ifndef VALUE_TYPE
#define VALUE_TYPE uint
#endif

void comp(local KEY_TYPE* key_a,
          local KEY_TYPE* key_b,
          local VALUE_TYPE* val_a,
          local VALUE_TYPE* val_b,
          bool dir)
{
  if( (*key_a > *key_b) == dir )
  {
    //swap
    KEY_TYPE kT = *key_a;
    *key_a = *key_b;
    *key_b = kT;

    VALUE_TYPE vT = *val_a;
    *val_a = *val_b;
    *val_b = vT;
  }
}


void bitonicSort(uint linId,
                 local KEY_TYPE *keys,
                 local VALUE_TYPE *values,
                 uint dataSize,
                 bool dir)
{
  for(uint size = 2; size < dataSize; size <<= 1)
  {
    //Bitonic merge
    bool d = dir ^ ( (linId & (size / 2)) != 0 );
    for(uint stride = size / 2; stride > 0; stride >>= 1)
    {
      barrier(CLK_LOCAL_MEM_FENCE);
      uint pos = 2 * linId - (linId & (stride - 1));
      comp(keys + pos, keys + (pos + stride),
            values + pos, values + (pos + stride),
            d);
    }
  }

  //final merge
  for(uint stride = dataSize / 2; stride > 0; stride >>= 1)
  {
      barrier(CLK_LOCAL_MEM_FENCE);
      uint pos = 2 * linId - (linId & (stride - 1));
      comp(keys + pos, keys + (pos + stride),
            values + pos, values + (pos + stride),
            dir);
  }

  barrier(CLK_LOCAL_MEM_FENCE);
}


__kernel void sortTest(global KEY_TYPE* keys,
                       global VALUE_TYPE* values,
                       local KEY_TYPE* l_keys,
                       local VALUE_TYPE* l_values)
{
  //copy data in
  uint linid = get_local_id(0);
  l_keys[linid]                       = keys[get_group_id(0)*2*get_local_size(0) + linid];
  l_keys[get_local_size(0) + linid]   = keys[get_group_id(0)*2*get_local_size(0) + get_local_size(0) + linid];
  l_values[linid]                     = values[get_group_id(0)*2*get_local_size(0) + linid];
  l_values[get_local_size(0) + linid] = values[get_group_id(0)*2*get_local_size(0) + get_local_size(0) + linid];

  ////sort
  bitonicSort(linid,l_keys,l_values,2*get_local_size(0),true);

  //write back
  keys[2*get_group_id(0)*get_local_size(0) + linid]                       = l_keys[linid];
  keys[2*get_group_id(0)*get_local_size(0) + get_local_size(0) + linid]   = l_keys[get_local_size(0) + linid];
  values[2*get_group_id(0)*get_local_size(0) + linid]                     = l_values[linid];
  values[2*get_group_id(0)*get_local_size(0) + get_local_size(0) + linid] = l_values[get_local_size(0) + linid];
}