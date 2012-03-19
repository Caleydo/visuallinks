#include "gpurouting.h"
#include "log.hpp"

#include <iostream>
#include <fstream>
#include <cmath>
#include <set>
#include <algorithm>
#include "datatypes.h"


#if defined(__APPLE__) || defined(__MACOSX)
# error "Context sharing not supported yet."
#elif defined(_WIN32)
//# include <GL/wgl.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <direct.h>
#define GetCurrentDir _getcwd
#else
# include <GL/glx.h>
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#define INLINE_TEXT(txt) #txt

namespace LinksRouting
{

  //----------------------------------------------------------------------------
  GPURouting::GPURouting() :
        myname("GPURouting")
  {
    registerArg("BlockSizeX", _blockSize[0] = 8);
    registerArg("BlockSizeY", _blockSize[1] = 8);
    registerArg("enabled", _enabled = true);
  }

  //------------------------------------------------------------------------------
  void GPURouting::publishSlots(SlotCollector& slots)
  {

  }

  //----------------------------------------------------------------------------
  void GPURouting::subscribeSlots(SlotSubscriber& slot_subscriber)
  {
    _subscribe_costmap =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/costmap");
    _subscribe_desktop =
      slot_subscriber.getSlot<LinksRouting::SlotType::Image>("/desktop");
    _subscribe_links =
      slot_subscriber.getSlot<LinkDescription::LinkList>("/links");
  }

  bool GPURouting::startup(Core* core, unsigned int type)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  void GPURouting::init()
  {
    _link_infos.clear();
  }

  //----------------------------------------------------------------------------
  void GPURouting::initGL()
  {
    // -----------------------------
    // OpenCL platform

    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if( platforms.empty() )
      throw std::runtime_error("Got no OpenCL platform.");

    //TODO: select devices to use
    cl::Platform use_platform;
    std::vector<cl::Device> use_devices;
    //for now:
    use_platform = platforms.front();

    //for (cl::Platform &tplatform : platforms)
    for(size_t i = 0; i < platforms.size(); ++i)
    {
      cl::Platform &tplatform = platforms[i];

      const std::string extensions =
        tplatform.getInfo<CL_PLATFORM_EXTENSIONS>();

      std::cout << "OpenCL Platform Info:"
                << "\n -- platform:\t " << tplatform.getInfo<CL_PLATFORM_NAME>()
                << "\n -- version:\t " << tplatform.getInfo<CL_PLATFORM_VERSION>()
                << "\n -- vendor:\t " << tplatform.getInfo<CL_PLATFORM_VENDOR>()
                << "\n -- profile:\t " << tplatform.getInfo<CL_PLATFORM_PROFILE>()
                << std::endl;

      std::vector<cl::Device> devices;
      tplatform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
      //for (cl::Device &tdevice : devices)
      for (size_t j = 0; j < devices.size(); ++j)
      {
         cl::Device &tdevice = devices[j];
         const std::string dextensions = tdevice.getInfo<CL_DEVICE_EXTENSIONS>();
         std::cout << "\tDevice Info:"
                   << "\n\t -- clock freq:\t " << tdevice.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>()
                   << "\n\t -- mem size:\t " << tdevice.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()
                   << "\n\t -- extensions:\t " << dextensions
                   << std::endl;
         //for now (use first only)
         if(i == 0)
#if defined(__APPLE__) || defined(__MACOSX)
         if( dextensions.find("cl_apple_gl_sharing") != std::string::npos )
#else
         if( extensions.find("cl_khr_gl_sharing") != std::string::npos )
#endif
         {
           use_devices.clear();
           use_devices.push_back(tdevice);
           break;
         }
      }

    }


    // -----------------------------
    // OpenCL context

    std::vector<cl_context_properties> properties;

#if defined(__APPLE__) || defined(__MACOSX)

# error "Not implemented yet."

    // TODO check
    properties.push_back( CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE );
    properties.push_back( (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) );
#else

# ifdef _WIN32
    properties.push_back( CL_GL_CONTEXT_KHR );
    properties.push_back( (cl_context_properties)wglGetCurrentContext() );

    properties.push_back( CL_WGL_HDC_KHR );
    properties.push_back( (cl_context_properties)wglGetCurrentDC() );
# else
    properties.push_back( CL_GLX_DISPLAY_KHR );
    properties.push_back( (cl_context_properties)glXGetCurrentDisplay() );

    properties.push_back( CL_GL_CONTEXT_KHR );
    properties.push_back( cl_context_properties(glXGetCurrentContext()) );
# endif
#endif

    properties.push_back( CL_CONTEXT_PLATFORM );
    properties.push_back( (cl_context_properties)use_platform() );
    properties.push_back(0);

    _cl_context = cl::Context(use_devices, &properties[0]);

    //_cl_context = cl::Context(CL_DEVICE_TYPE_GPU, &properties[0]);

    // -----------------------------
    // OpenCL device

    std::vector<cl::Device> devices = _cl_context.getInfo<CL_CONTEXT_DEVICES>();

    if( devices.empty() )
      throw std::runtime_error("Got no OpenCL device.");
    _cl_device = devices[0];

    std::cout << "OpenCL Device Info:"
              << "\n -- type:\t " << _cl_device.getInfo<CL_DEVICE_TYPE>()
              << "\n -- vendor:\t " << _cl_device.getInfo<CL_DEVICE_VENDOR>()
              << "\n -- name:\t " << _cl_device.getInfo<CL_DEVICE_NAME>()
              << "\n -- profile:\t " << _cl_device.getInfo<CL_DEVICE_PROFILE>()
              << "\n -- device version:\t " << _cl_device.getInfo<CL_DEVICE_VERSION>()
              << "\n -- driver version:\t " << _cl_device.getInfo<CL_DRIVER_VERSION>()
              << "\n -- compute units:\t " << _cl_device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()
              << "\n -- workgroup size:\t " << _cl_device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()
              << "\n -- global memory:\t " << (_cl_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() >> 20) << " MiB"
              << "\n -- local memory:\t " << (_cl_device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() >> 10) << " KiB"
              << "\n -- clock frequency:\t " << _cl_device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>()
              << std::endl;


    _cl_command_queue = cl::CommandQueue(_cl_context, _cl_device, CL_QUEUE_PROFILING_ENABLE);

    // -----------------------------
    // And now the OpenCL program


    std::ifstream source_file("routing.cl");
    if( !source_file )
      throw std::runtime_error("Failed to open routing.cl");
    std::string source( std::istreambuf_iterator<char>(source_file),
                       (std::istreambuf_iterator<char>()) );

    //now handled via include
    //std::ifstream source_file2("sorting.cl");
    //if( !source_file2 )
    //  throw std::runtime_error("Failed to open sorting.cl");
    //std::string source2( std::istreambuf_iterator<char>(source_file2),
    //                   (std::istreambuf_iterator<char>()) );

    cl::Program::Sources sources;
    sources.push_back( std::make_pair(source.c_str(), source.length()) );
    //sources.push_back( std::make_pair(source2.c_str(), source2.length()) );

    _cl_program = cl::Program(_cl_context, sources);

    char c_cdir[1024];
    if (!GetCurrentDir(c_cdir, 1024))
      throw std::runtime_error("Failed to retrieve current working directory");
    try
    {
      std::string buildargs;
      buildargs += "-cl-fast-relaxed-math";
      buildargs += " -I ";
      buildargs += c_cdir;
      //std::cout << buildargs << std::endl;
      _cl_program.build(devices, buildargs.c_str());
    }
    catch(cl::Error& ex)
    {
      std::cerr << "Failed to build OpenCL program: "
                << " -- Log: " << _cl_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(_cl_device)
                << std::endl;
      throw;
    }

    std::cout << "OpenCL Program Build:"
              << "\n -- Status:\t" << _cl_program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(_cl_device)
              << "\n -- Options:\t" << _cl_program.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(_cl_device)
              << "\n -- Log: " << _cl_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(_cl_device)
              << std::endl;

    _cl_clearQueueLink_kernel = cl::Kernel(_cl_program, "clearQueueLink");
    _cl_prepare_kernel = cl::Kernel(_cl_program, "prepareRouting");
    _cl_shortestpath_kernel = cl::Kernel(_cl_program, "routing");
    _cl_getMinimum_kernel = cl::Kernel(_cl_program, "getMinimum");
    _cl_routeInOut_kernel = cl::Kernel(_cl_program, "calcInOut");
    _cl_routeInterBlock_kernel = cl::Kernel(_cl_program, "calcInterBlockRoute");

   // //test
   // int blocksize = 64;
   // int datasize = 4*blocksize;
   // cl::Kernel sorting_test = cl::Kernel(_cl_program, "sortTest");
   // cl::Buffer keys(_cl_context, CL_MEM_READ_WRITE, datasize*sizeof(cl_float));
   // cl::Buffer values(_cl_context, CL_MEM_READ_WRITE, datasize*sizeof(cl_uint));
   // std::vector<float> h_keys;
   // std::vector<int> h_values;
   // h_keys.reserve(datasize);
   // h_values.reserve(datasize);
   // for(int i = 0; i < datasize; ++i)
   // {
   //   int v = rand();
   //   h_keys.push_back(v/1000.0f);
   //   h_values.push_back(v);
   // }

   // _cl_command_queue.enqueueWriteBuffer(keys, true, 0, datasize*sizeof(cl_float), &h_keys[0]);
   // _cl_command_queue.enqueueWriteBuffer(values, true, 0, datasize*sizeof(cl_uint), &h_values[0]);

   // sorting_test.setArg(0, keys);
   // sorting_test.setArg(1, values);
   // sorting_test.setArg(2, 2*blocksize*sizeof(cl_float), NULL);
   // sorting_test.setArg(3, 2*blocksize*sizeof(cl_uint), NULL);

   //_cl_command_queue.enqueueNDRangeKernel
   // (
   //   sorting_test,
   //   cl::NullRange,
   //   cl::NDRange(datasize/2),
   //   cl::NDRange(blocksize)
   // );

   // _cl_command_queue.finish();
   // _cl_command_queue.enqueueReadBuffer(keys, true, 0, datasize*sizeof(cl_float), &h_keys[0]);
   // _cl_command_queue.enqueueReadBuffer(values, true, 0, datasize*sizeof(cl_uint), &h_values[0]);

   // for(int i = 0; i < datasize; ++i)
   //   std::cout <<  "<" << h_keys[i] << "," << h_values[i] << ">,\n";
   // std::cout << std::endl;
  }
  void GPURouting::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  void GPURouting::process(Type type)
  {
    //----------------------
    // Many sanity checks :)

    if( !_subscribe_costmap->isValid() )
    {
      std::cerr << "GPURouting: No valid costmap received." << std::endl;
      return;
    }

    if( !_subscribe_desktop->isValid() )
    {
      std::cerr << "GPURouting: No valid desktop image received." << std::endl;
      return;
    }

    if( _subscribe_costmap->_data->type != SlotType::Image::OpenGLTexture )
    {
      std::cerr << "GPURouting: No OpenGL texture costmap received." << std::endl;
      return;
    }

    if(    !_subscribe_costmap->_data->width
        || !_subscribe_costmap->_data->height )
    {
      std::cerr << "GPURouting: Invalide costmap dimensions (=0)." << std::endl;
      return;
    }

    if( !_subscribe_links->isValid() )
    {
      LOG_INFO("No valid routing data available.");
      return;
    }

    try
    {

    //------------------------------
    // get buffer from opengl
    std::vector<cl::Memory> memory_gl;
    memory_gl.push_back
    (
      cl::Image2DGL
      (
         _cl_context,
         CL_MEM_READ_ONLY,
         GL_TEXTURE_2D,
         0,
         _subscribe_costmap->_data->id
       )
    );

    glFinish();
    _cl_command_queue.enqueueAcquireGLObjects(&memory_gl);

    // now start analyzing the links

    const LinkDescription::LinkList& links = *_subscribe_links->_data;
    unsigned int width = _subscribe_costmap->_data->width,
              height = _subscribe_costmap->_data->height,
              num_points = width * height;
    int dim[] = {width, height, num_points};
    int downsample = _subscribe_desktop->_data->width / _subscribe_costmap->_data->width;
    unsigned int numBlocks[2] = {divup<unsigned>(width,_blockSize[0]),divup<unsigned>(height,_blockSize[1])};
    unsigned int sumBlocks = numBlocks[0]*numBlocks[1];

    for( auto it = links.begin(); it != links.end(); ++it )
    {
      auto info = _link_infos.find(it->_id);

      
      if(    info != _link_infos.end()
          && info->second._stamp == it->_stamp
          && info->second._revision == it->_link.getRevision() )
        //TODO: check if screen has changed -> update
        continue;

      LOG_INFO("NEW DATA to route: " << it->_id);

      // --------------------------------------------------------
    
      if( !_enabled ) // This can be set from the config file...
        return;

      //ROUTING
      cl::Buffer queue_info(_cl_context, CL_MEM_READ_WRITE, sizeof(cl_QueueGlobal));
      cl::Buffer idbuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(cl_ushort2)*256);

      std::vector<int4> h_targets;
      h_targets.reserve(it->_link.getNodes().size());
      for( auto node = it->_link.getNodes().begin();
           node != it->_link.getNodes().end();
           ++node )
      {
        //std::cout << "Polygon: (" << node->getVertices().size() << " points)\n";
        //compute min max for poly
        int4 target(width-1, height-1,0,0);

        for( auto p = node->getVertices().begin();
             p != node->getVertices().end();
             ++p )
        {
          target.x = std::min(target.x, p->x/downsample);
          target.y = std::min(target.y, p->y/downsample);
          target.z = std::max(target.z, p->x/downsample);
          target.w = std::max(target.w, p->y/downsample);
        }
 
        target.x = std::max(target.x, 0);
        target.y = std::max(target.y, 0);
        target.z = std::min(target.z, ((int)width-1));
        target.w = std::min(target.w, ((int)height-1));

        if(target.z - target.x <= 0 || target.w - target.y <= 0)
          continue;
        h_targets.push_back(target);
      }

      //remove duplicates
      std::set<int4> dups(h_targets.begin(), h_targets.end());
      h_targets.swap(std::vector<int4>(dups.begin(), dups.end()));

      if(h_targets.size() > 1)
      {
        // there is something on screen -> call routing
            
        // setup queue
        int numtargets = h_targets.size();

        unsigned int overAllBlocks = sumBlocks * numtargets;
        unsigned int blockProcessThreshold = overAllBlocks*40;
        cl::Buffer buf(_cl_context, CL_MEM_READ_WRITE, num_points * numtargets * sizeof(float));

        cl::Buffer queue_priority(_cl_context, CL_MEM_READ_WRITE, overAllBlocks * sizeof(cl_float));
        cl::Buffer queue(_cl_context, CL_MEM_READ_WRITE, overAllBlocks *sizeof(cl_QueueElement));
        cl::Buffer targets(_cl_context, CL_MEM_READ_WRITE,numtargets*sizeof(cl_uint4));
        unsigned int blockborder =  2*(_blockSize[0] + _blockSize[1] - 2);
        cl::Buffer routing_block_inout(_cl_context, CL_MEM_READ_WRITE, numtargets*(1 + sumBlocks * blockborder ) * sizeof(cl_uint));

        _cl_command_queue.enqueueWriteBuffer(targets, true, 0, numtargets*sizeof(cl_uint4), &h_targets[0]);

        cl_QueueGlobal baseQueueInfo(std::min<int>(overAllBlocks + 1, 128*1024));
        _cl_command_queue.enqueueWriteBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);

        //clear data
        _cl_clearQueueLink_kernel.setArg(0, queue_priority);
        cl::Event clearQueueLink_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_clearQueueLink_kernel,
          cl::NullRange,
          cl::NDRange(numBlocks[0]*numBlocks[1]*numtargets),
          cl::NullRange,
          0,
          &clearQueueLink_Event
        );


        //insert nodes and prepare data
        _cl_prepare_kernel.setArg(0, memory_gl[0]);
        _cl_prepare_kernel.setArg(1, buf);
        _cl_prepare_kernel.setArg(2, queue);
        _cl_prepare_kernel.setArg(3, queue_priority);
        _cl_prepare_kernel.setArg(4, queue_info);
        _cl_prepare_kernel.setArg(5, sizeof(unsigned int), &overAllBlocks);
        _cl_prepare_kernel.setArg(6, targets);
        _cl_prepare_kernel.setArg(7, sizeof(int), &numtargets);
        _cl_prepare_kernel.setArg(8, 2 * sizeof(int), dim);
        _cl_prepare_kernel.setArg(9, 2 * sizeof(int), numBlocks);

        cl::Event prepare_kernel_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_prepare_kernel,
          cl::NullRange,
          cl::NDRange(_blockSize[0]*numBlocks[0],_blockSize[1]*numBlocks[1],numtargets),
          cl::NDRange(_blockSize[0], _blockSize[1], 1),
          0,
          &prepare_kernel_Event
        );

        //compute shortest paths
        size_t localmem = std::max
        (
          sizeof(cl_float) * (_blockSize[0] + 2) * (_blockSize[1] + 2),
          _blockSize[0] * _blockSize[1] * 2 * (sizeof(cl_float) + sizeof(cl_uint))
        );
        _cl_shortestpath_kernel.setArg(0, memory_gl[0]);
        _cl_shortestpath_kernel.setArg(1, buf);
        _cl_shortestpath_kernel.setArg(2, queue);
        _cl_shortestpath_kernel.setArg(3, queue_priority);
        _cl_shortestpath_kernel.setArg(4, queue_info);
        _cl_shortestpath_kernel.setArg(5, sizeof(unsigned int), &overAllBlocks);
        _cl_shortestpath_kernel.setArg(6, 2 * sizeof(int), dim);
        _cl_shortestpath_kernel.setArg(7, 2 * sizeof(int), numBlocks);
        _cl_shortestpath_kernel.setArg(8, sizeof(unsigned int), &blockProcessThreshold);
        _cl_shortestpath_kernel.setArg(9, localmem, NULL);

        cl::Event shortestpath_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_shortestpath_kernel,
          cl::NullRange,
          cl::NDRange(_blockSize[0]*numBlocks[0],_blockSize[1]*numBlocks[1],numtargets),
          cl::NDRange(_blockSize[0], _blockSize[1], 1),
          0,
          &shortestpath_Event
        );

 

        //_cl_command_queue.enqueueReadBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);
        //std::cout << "front: " << baseQueueInfo.front << " "
        //          << "back: " << baseQueueInfo.back << " "
        //          << "filllevel: " << baseQueueInfo.filllevel << " "
        //          << "activeBlocks: " << baseQueueInfo.activeBlocks << " "
        //          << "processedBlocks: " << baseQueueInfo.processedBlocks << " "
        //          << "debug: " << baseQueueInfo.debug << "\n";
        //std::vector<cl_QueueElement> test(baseQueueInfo.back-baseQueueInfo.front);
        //_cl_command_queue.enqueueReadBuffer(queue, true, sizeof(cl_QueueElement)*baseQueueInfo.front, sizeof(cl_QueueElement)*(baseQueueInfo.back-baseQueueInfo.front), &test[0]);
        //for(int i = 0; i < test.size(); ++i)
        //{
        //  std::cout << test[i].block.s[0] << " "
        //            << test[i].block.s[1] << " "
        //            << test[i].node << " "
        //            << test[i].priority << "\n";
        //}
        //_cl_command_queue.finish();


        //search for minimum

        _cl_command_queue.finish();

        _cl_getMinimum_kernel.setArg(0, buf);
        _cl_getMinimum_kernel.setArg(1, queue_info);
        _cl_getMinimum_kernel.setArg(2, idbuffer);
        _cl_getMinimum_kernel.setArg(3, 2 * sizeof(cl_int), dim);
        _cl_getMinimum_kernel.setArg(4, numtargets);

        cl::Event getmin_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_getMinimum_kernel,
          cl::NullRange,
          cl::NDRange(dim[0],dim[1]),
          cl::NullRange,
          0,
          &getmin_Event
        );

        _cl_command_queue.finish();

        cl_ushort2 us2_dummy;
        std::vector<cl_ushort2> h_idbuffer(256, us2_dummy);
        _cl_command_queue.enqueueReadBuffer(idbuffer, true, 0, sizeof(cl_ushort2)*256, &h_idbuffer[0]);
        _cl_command_queue.enqueueReadBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);
        cl_uint cleanedcost = (baseQueueInfo.mincost & 0xFFFFFF00) >> 1;
        cl_uint idbufferoffset = baseQueueInfo.mincost&0xFF;
        int startingpoint[2] = {h_idbuffer[idbufferoffset].s[0],
                                h_idbuffer[idbufferoffset].s[1]};
        //debug
        std::cout << "front: " << baseQueueInfo.front << "\n"
                  << "back: " << baseQueueInfo.back << "\n"
                  << "filllevel: " << baseQueueInfo.filllevel << "\n"
                  << "activeBlocks: " << baseQueueInfo.activeBlocks << "\n"
                  << "processedBlocks: " << baseQueueInfo.processedBlocks << "\n"
                  << "min: " <<  *(float*)&cleanedcost << " @ " <<  startingpoint[0] << "," << startingpoint[1] << "\n"
                  << "debug: " << baseQueueInfo.debug << "\n\n";
        //-----

        //local route reconstruction
        _cl_routeInOut_kernel.setArg(0, buf);
        _cl_routeInOut_kernel.setArg(1, routing_block_inout);
        _cl_routeInOut_kernel.setArg(2, _blockSize[0]*_blockSize[1]*sizeof(cl_int), NULL);
        _cl_routeInOut_kernel.setArg(3, _blockSize[0]*_blockSize[1]*sizeof(cl_int), NULL);
        _cl_routeInOut_kernel.setArg(4, 2 * sizeof(cl_int), startingpoint);
        _cl_routeInOut_kernel.setArg(5, 2 * sizeof(cl_int), dim);

        cl::Event routeInOut_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_routeInOut_kernel,
          cl::NullRange,
          cl::NDRange(_blockSize[0]*numBlocks[0],_blockSize[1]*numBlocks[1],numtargets),
          cl::NDRange(_blockSize[0], _blockSize[1], 1),
          0,
          &routeInOut_Event
        );

        _cl_command_queue.finish();

        //inter block route reconstruction
        

        _cl_command_queue.enqueueReleaseGLObjects(&memory_gl);
        _cl_command_queue.finish();

        cl_ulong start, end;
        std::cout << "CLInfo:\n";
        clearQueueLink_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        clearQueueLink_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - clearing data: " << (end-start)/1000000.0 << "ms\n";
        prepare_kernel_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        prepare_kernel_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - preparing data: " << (end-start)/1000000.0  << "ms\n";
        shortestpath_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        shortestpath_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - routing: " << (end-start)/1000000.0  << "ms\n";
        getmin_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        getmin_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - get minimum: " << (end-start)/1000000.0  << "ms\n";
        routeInOut_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        routeInOut_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - local route reconstruction: " << (end-start)/1000000.0  << "ms\n";

        //std::vector<float> host_mem(num_points*numtargets);
        //_cl_command_queue.enqueueReadBuffer(buf, true, 0, num_points * numtargets * sizeof(float), &host_mem[0]);

        //std::ofstream fimg("test.pfm", std::ios_base::out | std::ios_base::binary | std::ios_base::trunc );
        //fimg << "PF\n";
        //fimg << width << " " << height*numtargets << '\n';
        //fimg << -1.0f << '\n';

        ////std::ofstream img("test.pgm", std::ios_base::trunc);
        ////img << "P2\n"
        ////    << width << " " << height*numtargets << "\n"
        ////    << "255\n";

        //for( unsigned int i = 0; i < num_points*numtargets; ++i )
        //{
        //  //img << host_mem[i] << "\n";
        //  for(int j = 0; j < 3; ++j)
        //    fimg.write(reinterpret_cast<char*>(&host_mem[i]),sizeof(float));
        //}
      }

      // --------------------------------------------------------

      // set as handled
      if( info == _link_infos.end() )
        _link_infos[it->_id] = LinkInfo(it->_stamp, it->_link.getRevision());
      else
      {
        info->second._stamp = it->_stamp;
        info->second._revision = it->_link.getRevision();
      }
    }
    }
    catch(cl::Error& err)
    {
      std::cerr << err.err() << "->" << err.what() << std::endl;
    }
  }

  void GPURouting::connect(CostAnalysis* costanalysis,
                           LinksRouting::Renderer *renderer)
  {

  }

  bool GPURouting::addLinkHierarchy(LinkDescription::Node* node,
                                    double priority)
  {
    return true;
  }
  bool GPURouting::addLinkHierarchy(LinkDescription::HyperEdge* hyperedge,
                                    double priority)
  {
    return true;
  }
  bool GPURouting::removeLinkHierarchy(LinkDescription::Node* node)
  {
    return true;
  }
  bool GPURouting::removeLinkHierarchy(LinkDescription::HyperEdge* hyperedge)
  {
    return true;
  }

}
