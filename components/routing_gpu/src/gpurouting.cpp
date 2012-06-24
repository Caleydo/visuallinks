#include "gpurouting.h"
#include "log.hpp"

#include <iostream>
#include <fstream>
#include <cmath>
#include <set>
#include <algorithm>
#include <limits>
#include "datatypes.h"

#define USE_QUEUE_ROUTING 1

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
        myname("GPURouting"),
        _buffer_width(0),
        _buffer_height(0)
  {
    registerArg("BlockSizeX", _blockSize[0] = 8);
    registerArg("BlockSizeY", _blockSize[1] = 8);
    registerArg("enabled", _enabled = true);
    registerArg("NoQueue", _noQueue = false);
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




    _cl_updateRouteMap_kernel = cl::Kernel(_cl_program, "updateRouteMap");
    _cl_prepareBorderCostsX_kernel = cl::Kernel(_cl_program, "prepareBorderCostsX");
    _cl_prepareBorderCostsY_kernel = cl::Kernel(_cl_program, "prepareBorderCostsY");
    _cl_prepareIndividualRouting_kernel = cl::Kernel(_cl_program, "prepareIndividualRouting");

    _cl_getMinimum_kernel = cl::Kernel(_cl_program, "getMinimum");
    _cl_routeInOut_kernel = cl::Kernel(_cl_program, "calcInOut");
    _cl_routeInterBlock_kernel = cl::Kernel(_cl_program, "calcInterBlockRoute");
    _cl_routeConstruct_kernel = cl::Kernel(_cl_program, "constructRoute");
    

  }

  //----------------------------------------------------------------------------
  void GPURouting::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  template<typename T>
  void dumpBuffer( cl::CommandQueue& cl_queue,
                   const cl::Buffer& buf,
                   size_t width,
                   size_t height,
                   const std::string& file )
  {
//            cl_bool blocking,
//            ::size_t offset,
//            ::size_t size,
//            void* ptr
    cl_queue.finish();

    std::vector<T> mem(width * height);
    cl_queue.enqueueReadBuffer(buf, true, 0, width * height * sizeof(T), &mem[0]);

#if 1
    std::ofstream fimg(file + ".pfm", std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    fimg << "Pf\n";
    fimg << width << " " << height << '\n';
    fimg << -1.0f << '\n';

    for(auto it = mem.begin(); it != mem.end(); ++it)
      fimg.write(reinterpret_cast<char*>(&*it), sizeof(T));
#else
    std::ofstream img( (file + ".pgm").c_str(), std::ios_base::trunc);
    img << "P2\n"
        << width << " " << height << "\n"
        << "8191\n";

    auto minmax = std::minmax_element(mem.begin(), mem.end());
    T min = *minmax.first,
      range = 10;//*minmax.second - min;

    std::cout << file << "-> min=" << min << ", range=" << range << std::endl;

    for(auto it = mem.begin(); it != mem.end(); ++it)
      img << static_cast<int>(std::min(*it - min, 10.f) / range * 8191) << "\n";
#endif
  }

  //----------------------------------------------------------------------------
  template<typename T>
  std::vector<T> dumpRingBuffer( cl::CommandQueue& cl_queue,
                                   const cl::Buffer& buf,
                                   size_t front,
                                   size_t back,
                                   size_t max_len,
                                   const std::string& file )
  {
    cl_queue.finish();
    std::cout << "Dumping buffer to '" << file << "'" << std::endl;

    front = front % max_len;
    back = back % max_len;

    std::vector<T> mem;
    if( front < back )
    {
      mem.resize(back - front + 1);
      cl_queue.enqueueReadBuffer
      (
        buf,
        true,
        sizeof(T) * front,
        sizeof(T) * mem.size(),
        &mem[0]
      );
    }
    else
    {
      mem.resize(max_len - front + back + 1);
      cl_queue.enqueueReadBuffer
      (
        buf,
        true,
        sizeof(T) * front,
        sizeof(T) * (max_len - front),
        &mem[0]
      );
      cl_queue.enqueueReadBuffer
      (
        buf,
        true,
        0,
        sizeof(T) * back,
        &mem[max_len - front]
      );
    }

    std::ofstream strm(file);

    for(size_t i = 0; i < mem.size(); ++i)
    {
      strm << mem[i] << "\n";
    }

    return mem;
  }

  //----------------------------------------------------------------------------
  std::ostream& operator<<( std::ostream& strm,
                             const GPURouting::cl_QueueElement& el )
  {
    for(int i = 0; i < 4; ++i)
      strm << el.s[i] << " ";
    return strm;
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
      LOG_DEBUG("No valid routing data available.");
      return;
    }

    if( !_enabled ) // This can be set from the config file...
      return;

    try
    {



    updateRouteMap();

    // now start analyzing the links

    LinkDescription::LinkList& links = *_subscribe_links->_data;
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

      LOG_INFO("NEW DATA to route: "
               << it->_id
               << " using routing kernel with" <<  (_noQueue ? "out" : "")
               << " queue.");

      // --------------------------------------------------------

      createRoutes(it->_link);

#if 0
      {
      //ROUTING
      cl::Buffer queue_info(_cl_context, CL_MEM_READ_WRITE, sizeof(cl_QueueGlobal));
      cl::Buffer idbuffer(_cl_context, CL_MEM_READ_WRITE, sizeof(cl_ushort2)*256);


      std::vector<int4Aug<const LinkDescription::Node*> > h_targets;
      h_targets.reserve(it->_link.getNodes().size());
      for( auto node = it->_link.getNodes().begin();
           node != it->_link.getNodes().end();
           ++node )
      {
        if( node->getProps().find("hidden") != node->getProps().end() )
          continue;

        //std::cout << "Polygon: (" << node->getVertices().size() << " points)\n";
        //compute min max for poly
        int4Aug<const LinkDescription::Node*> target(width-1, height-1,0,0, &*node);

        for( auto p = node->getVertices().begin();
             p != node->getVertices().end();
             ++p )
        {
          target.x = std::min<int>(target.x, p->x/downsample);
          target.y = std::min<int>(target.y, p->y/downsample);
          target.z = std::max<int>(target.z, p->x/downsample);
          target.w = std::max<int>(target.w, p->y/downsample);
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
      std::set<int4Aug<const LinkDescription::Node*> > dups(h_targets.begin(), h_targets.end());
      h_targets = std::vector<int4Aug<const LinkDescription::Node*> >(dups.begin(), dups.end());

      int numtargets = h_targets.size();

      // prepare storage for result
      LinkDescription::HyperEdgeDescriptionForkation* fork = new LinkDescription::HyperEdgeDescriptionForkation();
      it->_link.setHyperEdgeDescription(fork);

      if(h_targets.size() < 2)
      {
        if( h_targets.empty() )
        {
          std::cout << "No target!" << std::endl;
        }
        else
        {
          std::cout << "Only 1 target!" << std::endl;
          // TODO do we need position?
//          fork->position.x = startingpoint[0] * downsample;
//          fork->position.y = startingpoint[1] * downsample;

          // copy only the single region
          fork->outgoing.push_back(LinkDescription::HyperEdgeDescriptionSegment());
          fork->outgoing.back().parent = fork;
          fork->outgoing.back().nodes.push_back(h_targets[0].aug);
        }
      }
      else
      {
        // there is something on screen -> call routing

        // setup queue
        unsigned int overAllBlocks = sumBlocks * numtargets;
        unsigned int blockProcessThreshold = overAllBlocks * 128;
        cl::Buffer buf(_cl_context, CL_MEM_READ_WRITE, num_points * numtargets * sizeof(float));

        cl::Buffer queue_priority(_cl_context, CL_MEM_READ_WRITE, overAllBlocks * sizeof(cl_float));
        cl::Buffer queue(_cl_context, CL_MEM_READ_WRITE, overAllBlocks *sizeof(cl_QueueElement));
        cl::Buffer targets(_cl_context, CL_MEM_READ_WRITE,numtargets*sizeof(cl_uint4));
        unsigned int blockborder =  2*(_blockSize[0] + _blockSize[1] - 2);
        cl::Buffer routing_block_inout(_cl_context, CL_MEM_READ_WRITE, numtargets*(1 + sumBlocks * blockborder ) * sizeof(cl_uint));
        cl::Buffer routing_inter_block(_cl_context, CL_MEM_READ_WRITE, numtargets*(1 + sumBlocks) * sizeof(cl_uint2));

        std::vector<int4> h_target_plain(h_targets.begin(), h_targets.end());
        _cl_command_queue.enqueueWriteBuffer(targets, true, 0, numtargets*sizeof(cl_uint4), &h_target_plain[0]);

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

        _cl_clearQueue_kernel.setArg(0, queue);
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_clearQueue_kernel,
          cl::NullRange,
          cl::NDRange(overAllBlocks),
          cl::NullRange,
          0,
          0
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

        dumpBuffer<float>(_cl_command_queue, buf, width, height * numtargets, "prepare");

        _cl_command_queue.finish();
        _cl_command_queue.enqueueReadBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);

        std::cout << "front: " << baseQueueInfo.front << " "
                  << "back: " << baseQueueInfo.back << " "
                  << "filllevel: " << baseQueueInfo.filllevel << " "
                  << "activeBlocks: " << baseQueueInfo.activeBlocks << " "
                  << "processedBlocks: " << baseQueueInfo.processedBlocks << " "
                  << "debug: " << baseQueueInfo.debug << "\n";

        //compute shortest paths
        size_t localmem = std::max
        (
          sizeof(cl_float) * (_blockSize[0] + 2) * (_blockSize[1] + 2),
          _blockSize[0] * _blockSize[1] * 2 * (sizeof(cl_float) + sizeof(cl_uint))
        );

        if( _noQueue )
        {
          _cl_shortestpath_kernel.setArg(0, memory_gl[0]);
          _cl_shortestpath_kernel.setArg(1, buf);
          _cl_shortestpath_kernel.setArg(2, 2 * sizeof(int), dim);
          _cl_shortestpath_kernel.setArg(3, localmem, NULL);
        }
        else
        {
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
        }

        cl::Event shortestpath_Event;
        cl_ulong routing_time = 0;
        size_t num_iterations = std::max(40/numtargets, 10) * std::max(numBlocks[0], numBlocks[1]);
        if( _noQueue )
          std::cout << "num_iterations=" << num_iterations << std::endl;
        for( int i = 0; i < (_noQueue ? num_iterations : 1); ++i )
        {
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_shortestpath_kernel,
            cl::NullRange,
            cl::NDRange(_blockSize[0]*numBlocks[0],_blockSize[1]*numBlocks[1],numtargets),
            cl::NDRange(_blockSize[0], _blockSize[1], 1),
            0,
            &shortestpath_Event
          );
          _cl_command_queue.finish();

          cl_ulong start, end;
          shortestpath_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          shortestpath_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          routing_time += end - start;
        }

        dumpBuffer<float>(_cl_command_queue, buf, width, height * numtargets, "shortestpath");

        _cl_command_queue.finish();
        _cl_command_queue.enqueueReadBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);

        std::cout << "front: " << baseQueueInfo.front << " "
                  << "back: " << baseQueueInfo.back << " "
                  << "filllevel: " << baseQueueInfo.filllevel << " "
                  << "activeBlocks: " << baseQueueInfo.activeBlocks << " "
                  << "processedBlocks: " << baseQueueInfo.processedBlocks << " "
                  << "debug: " << baseQueueInfo.debug << "\n";


        //_cl_command_queue.enqueueReadBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);
        //std::cout << "front: " << baseQueueInfo.front << " "
        //          << "back: " << baseQueueInfo.back << " "
        //          << "filllevel: " << baseQueueInfo.filllevel << " "
        //          << "activeBlocks: " << baseQueueInfo.activeBlocks << " "
        //          << "processedBlocks: " << baseQueueInfo.processedBlocks << " "
        //          << "debug: " << baseQueueInfo.debug << "\n";
        std::cout << "overAllBlocks=" << overAllBlocks << std::endl;

        std::vector<cl_QueueElement> queue_data =
          dumpRingBuffer<cl_QueueElement>
          (
            _cl_command_queue,
            queue,
            baseQueueInfo.front,
            baseQueueInfo.back,
            overAllBlocks,
            "queue.txt"
          );
        dumpRingBuffer<cl_float>
        (
          _cl_command_queue,
          queue_priority,
          baseQueueInfo.front,
          baseQueueInfo.back,
          overAllBlocks,
          "queue_priorities.txt"
        );
        dumpRingBuffer<cl_float>
        (
          _cl_command_queue,
          queue_priority,
          0,
          overAllBlocks - 1,
          overAllBlocks,
          "queue_priorities_all.txt"
        );

        for(size_t i = 0; i < h_targets.size(); ++i)
          std::cout << h_targets[i].x/_blockSize[0] << "->" <<  h_targets[i].z/_blockSize[0]
                    << " " << h_targets[i].y/_blockSize[1] << "->" <<  h_targets[i].w/_blockSize[1]
                    << std::endl;

        std::set<int4> cleanup;
        for(unsigned int i = 0; i < queue_data.size(); ++i)
          if(!cleanup.insert(int4(queue_data[i].s[0],queue_data[i].s[1],queue_data[i].s[2],0)).second)
          {
            std::cout << "duplicated queue entry: ";
            for(int j = 0; j < 4; ++j)
            std::cout << queue_data[i].s[j] << " ";
            std::cout << "\n";
          }

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
        //cl_uint cleanedcost = (baseQueueInfo.mincost & 0xFFFFFF00) >> 1;
        cl_uint idbufferoffset = baseQueueInfo.mincost&0xFF;
        int startingpoint[2] = {h_idbuffer[idbufferoffset].s[0],
                                h_idbuffer[idbufferoffset].s[1]};
        ////debug
        //std::cout << "front: " << baseQueueInfo.front << "\n"
        //          << "back: " << baseQueueInfo.back << "\n"
        //          << "filllevel: " << baseQueueInfo.filllevel << "\n"
        //          << "activeBlocks: " << baseQueueInfo.activeBlocks << "\n"
        //          << "processedBlocks: " << baseQueueInfo.processedBlocks << "\n"
        //          << "min: " <<  *(float*)&cleanedcost << " @ " <<  startingpoint[0] << "," << startingpoint[1] << "\n"
        //          << "debug: " << baseQueueInfo.debug << "\n\n";
        ////-----

        _cl_command_queue.finish();

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
        _cl_routeInterBlock_kernel.setArg(0, routing_block_inout);
        _cl_routeInterBlock_kernel.setArg(1, routing_inter_block);
        _cl_routeInterBlock_kernel.setArg(2, 2 * sizeof(cl_int), startingpoint);
        _cl_routeInterBlock_kernel.setArg(3, 2 * sizeof(cl_int), numBlocks);
        _cl_routeInterBlock_kernel.setArg(4, 2 * sizeof(cl_int), _blockSize);

        cl::Event routeInterBlock_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_routeInterBlock_kernel,
          cl::NullRange,
          cl::NDRange(numtargets),
          cl::NDRange(1),
          0,
          &routeInterBlock_Event
        );

        _cl_command_queue.finish();

        //copy info
        cl_uint2 ui2_dummy;
        std::vector<cl_uint2> interblockinfo(numtargets, ui2_dummy);
        _cl_command_queue.enqueueReadBuffer(routing_inter_block, true, 0, sizeof(cl_uint2)*numtargets, &interblockinfo[0]);

        //debug
        //std::vector<cl_uint2> interblockinfo(numtargets*(1 + sumBlocks), ui2_dummy);
        //_cl_command_queue.enqueueReadBuffer(routing_inter_block, true, 0, sizeof(cl_uint2)*numtargets*(1 + sumBlocks), &interblockinfo[0]);

        //int points = interblockinfo[0].s[1];
        //int blocks = interblockinfo[0].s[0];
        //for(int i = 0; i <= blocks; ++i)
        //{
        //  int cost = interblockinfo[numtargets + i].s[0];
        //  int px = interblockinfo[numtargets + i].s[1] >> 16;
        //  int py = interblockinfo[numtargets + i].s[1] &0xFFFF;

        //  std::cout << cost << " -> (" << px << "," << py << ")\n";
        //}
        //

        int maxpoints = 0, maxblocks = 0;
        for(int i = 0; i < numtargets; ++i)
        {
          maxpoints = std::max<int>(maxpoints, interblockinfo[i].s[1]);
          maxblocks = std::max<int>(maxblocks, interblockinfo[i].s[0]);
        }

        //these are the routes! the maxpoints info will also be needed!
        cl::Buffer routes(_cl_context, CL_MEM_READ_WRITE,maxpoints*sizeof(cl_int2)*numtargets);

        _cl_routeConstruct_kernel.setArg(0, buf);
        _cl_routeConstruct_kernel.setArg(1, routing_inter_block);
        _cl_routeConstruct_kernel.setArg(2, routes);
        _cl_routeConstruct_kernel.setArg(3, maxblocks);
        _cl_routeConstruct_kernel.setArg(4, maxpoints);
        _cl_routeConstruct_kernel.setArg(5, numtargets);
        _cl_routeConstruct_kernel.setArg(6, 2 * sizeof(cl_int), dim);
        _cl_routeConstruct_kernel.setArg(7, 2 * sizeof(cl_int), numBlocks);

        cl::Event routeConstruct_Event;
        _cl_command_queue.enqueueNDRangeKernel
        (
          _cl_routeConstruct_kernel,
          cl::NullRange,
          cl::NDRange(numtargets*maxblocks),
          cl::NullRange,
          0,
          &routeConstruct_Event
        );

        _cl_command_queue.finish();
        //copy results to cpu
        std::vector<cl_int2> outroutes(maxpoints * numtargets);
        _cl_command_queue.enqueueReadBuffer(routes, true, 0, maxpoints*sizeof(cl_int2)*numtargets, &outroutes[0]);

        //copy routes to hyperedge
        fork->position.x = startingpoint[0] * downsample;
        fork->position.y = startingpoint[1] * downsample;
        for(int i = 0; i < numtargets; ++i)
        {
          fork->outgoing.push_back(LinkDescription::HyperEdgeDescriptionSegment());
          fork->outgoing.back().parent = fork;
          fork->outgoing.back().trail.reserve(interblockinfo[i].s[1]);
          for(unsigned int j = 0; j < interblockinfo[i].s[1]; ++j)
          {
            fork->outgoing.back().trail.push_back
            (
              float2( outroutes[i*maxpoints +j].s[0] * downsample,
                      outroutes[i*maxpoints +j].s[1] * downsample )
            );
          }
          fork->outgoing.back().nodes.push_back(h_targets[i].aug);
        }

        cl_ulong start, end;
        std::cout << "CLInfo:\n";
        clearQueueLink_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        clearQueueLink_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - clearing data: " << (end-start)/1000000.0 << "ms\n";
        prepare_kernel_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        prepare_kernel_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - preparing data: " << (end-start)/1000000.0  << "ms\n";
        std::cout << " - routing: " << routing_time/1000000.0  << "ms\n";
        getmin_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        getmin_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - get minimum: " << (end-start)/1000000.0  << "ms\n";
        routeInOut_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        routeInOut_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - local route reconstruction: " << (end-start)/1000000.0  << "ms\n";
        routeInterBlock_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        routeInterBlock_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - inter block route reconstruction: " << (end-start)/1000000.0  << "ms\n";
        routeConstruct_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
        routeConstruct_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
        std::cout << " - route construction: " << (end-start)/1000000.0  << "ms\n";
      }
      }
#endif

      // --------------------------------------------------------

      // set as handled
      if( info == _link_infos.end() )
        _link_infos[it->_id] = LinkInfo(it->_stamp, it->_link.getRevision());
      else
      {
        info->second._stamp = it->_stamp;
        info->second._revision = it->_link.getRevision();
      }

      //throw std::runtime_error("Done routing!");
    }


    }
    catch(cl::Error& err)
    {
      std::cerr << err.err() << "->" << err.what() << std::endl;
      throw;
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




  //kernel test

struct int2
{
  union
  {
  int d[2];
  struct {  int x, y; };
  };
  int2() { }
  int2(int _x, int _y) : x(_x), y(_y) { }
  int2 operator + (const int2& other)
  {
    return int2(x + other.x, y + other.y);
  }
  int2 operator - (const int2& other)
  {
    return int2(x - other.x, y - other.y);
  }
  int& operator [] (int i)
  {
    return d[i];
  }
  const  int& operator [] (int i) const
  {
    return d[i];
  }
};

int2 local_size;
int2 local_id;
int2 global_id;
int2 global_size;
int2 num_groups;
int2 group_id;

void initKernelData(int2 lsize, int2 groups )
{
  local_size = lsize;
  local_id = int2(0,0);
  global_size = int2(lsize.x*groups.x, lsize.y*groups.y);
  global_id = int2(0,0);
  num_groups = groups;
  group_id = int2(0,0);
}

unsigned int get_local_size(int i)
{
  return local_size[i];
}
unsigned int get_local_id(int i)
{
  return local_id[i];
}
unsigned int get_global_size(int i)
{
  return global_size[i];
}
unsigned int get_global_id(int i)
{
  return global_id[i];
}
unsigned int get_group_id(int i)
{
  return group_id[i];
}
unsigned int get_num_groups(int i)
{
  return num_groups[i];
}
bool advanceThread()
{
  ++global_id.x;
  if(++local_id.x >= local_size.x)
  {
    global_id.x -= local_size.x;
    local_id.x = 0;
    ++global_id.y;
    if(++local_id.y >= local_size.y)
    {
      local_id.y = 0;
      global_id.y -= local_size.y;
      return false;
    }
  }
  return true;
}


int2 costmap_dim;
float getPenalty(float* costmap, int2 pos)
{
  return costmap[pos.x + pos.y*costmap_dim.x];
}


float readLastPenalty( float* lastCostmap, int2 pos, const int2 size)
{
  return lastCostmap[pos.x + pos.y*size.x];
}

void writeLastPenalty( float* lastCostmap, int2 pos, const int2 size, float data)
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
    return int2(-1,-1);
  if(borderId <  lsize.x)
    return int2(borderId, 0);
  if(borderId < lsize.x + lsize.y - 1)
    return int2(lsize.x-1, borderId - (lsize.x-1));
  if(borderId < 2*lsize.x + lsize.y - 2)
    return int2(2*lsize.x+lsize.y-3-borderId , lsize.y-1);
  else
    return int2(0, 2*(lsize.x + lsize.y - 2) - borderId);
}

float* accessLocalCost(float* l_costs, int2 id)
{
  return l_costs + (id.x+1 + (id.y+1)*(get_local_size(0)+2));
}

const float* accessLocalCostConst(float const * l_costs, int2 id)
{
  return l_costs + (id.x+1 + (id.y+1)*(get_local_size(0)+2));
}

bool route_data(float const * l_cost,
                float* l_route)
{
  //route as long as there is change
  bool change;
  change = true;


  //iterate over it
  int counter = -1;
  while(change)
  {
    change = false;
    do
    {
    int2 localid = int2(get_local_id(0),get_local_id(1));
    float* myval = accessLocalCost(l_route, localid);

    float lastmyval = *myval;
    
    int2 offset;
    for(offset.y = -1; offset.y  <= 1; ++offset.y)
      for(offset.x = -1; offset.x <= 1; ++offset.x)
      {
        //d is either 1 or M_SQRT2 - maybe it is faster to substitute
        float d = sqrt((float)(offset.x*offset.x+offset.y*offset.y));
        //by
        //int d2 = offset.x*offset.x+offset.y*offset.y;
        //float d = (d2-1)*M_SQRT2 + (2-d2);
        float penalty = 0.5f*(*accessLocalCostConst(l_cost, localid) +
                              *accessLocalCostConst(l_cost, localid + offset));
        float r_other = *accessLocalCost(l_route, localid + offset);
        //TODO use custom params here
        *myval = std::min(*myval, r_other + 0.01f*d + d*penalty);
      }

    if(*myval != lastmyval)
      change = true;
    }
    while(advanceThread());
    ++counter;
  }

  return counter > 0;
}

void updateRouteMapDummy(float * costmap,
                    float* lastCostmap,
                    float* routeMap,
                    const int2 dim,
                    int computeAll,
                    float* l_data)
{
  const float MAXFLOAT = std::numeric_limits<float>::max();
  bool changed;
  changed = computeAll;

  float* l_cost = l_data;
  float* l_routing_data = l_data + (get_local_size(0)+2)*(get_local_size(1)+2);

  do
  {
  int lid = get_local_id(0) + get_local_id(1)*get_local_size(0);
  for(; lid < (get_local_size(0)+2)*(get_local_size(1)+2); lid += get_local_size(0)*get_local_size(1))
  {
    float newPenalty = 0.1f*MAXFLOAT;
    l_routing_data[lid] = 0.1f*MAXFLOAT;
    int2 lid2 = int2(lid%(get_local_size(0)+2),  (lid/(get_local_size(0)+2)));
    int2 gid2 = int2((int)(get_group_id(0)*get_local_size(0)) - 1 + lid2.x,
                       (int)(get_group_id(1)*get_local_size(1)) - 1 + lid2.y);

    if(gid2.x > 0 && gid2.x < dim.x && gid2.y > 0 && gid2.y < dim.y 
       && lid2.x <= get_local_size(0) && lid2.y <= get_local_size(1))
    {
      newPenalty = getPenalty(costmap, gid2);
      float oldPenalty = readLastPenalty(lastCostmap, gid2, dim);
      if( fabs(newPenalty - oldPenalty) > 0.01f )
      {
        changed = true;
        writeLastPenalty(lastCostmap, gid2, dim, newPenalty);
      }
    }
    l_cost[lid] = newPenalty;
  }
  } while(advanceThread());
  if(!changed) return;


  //do the routing
  int boundaryelements = 2*(get_local_size(0) + get_local_size(1));
  int written = 0;
  for(int i = 0; i < boundaryelements; ++i)
  {
    do
    {
    int lid = get_local_id(0) + get_local_id(1)*get_local_size(0);
    int2 borderId2 = lidForBorderId(lid, int2(get_local_size(0)+1,get_local_size(1)+1)) - int2(1,1);
    //init routing data    
    *accessLocalCost(l_routing_data, int2(get_local_id(0), get_local_id(1))) = 0.1f*MAXFLOAT;
    if(lid == i - 1)
      *accessLocalCost(l_routing_data, borderId2) = 0.1f*MAXFLOAT;
    if(lid == i)
      *accessLocalCost(l_routing_data, borderId2) = 0;
    } while(advanceThread());
    //route
    route_data(l_cost, l_routing_data);

    //write result
    int write = boundaryelements - i;
    do
    {
    int lid = get_local_id(0) + get_local_id(1)*get_local_size(0);
    int2 borderId2 = lidForBorderId(lid, int2(get_local_size(0)+1,get_local_size(1)+1)) - int2(1,1);
    if(lid >= 0 && lid < write) 
      routeMap[boundaryelements*boundaryelements/2*(get_group_id(0)+get_group_id(1)*get_num_groups(0)) +
               written + lid] = *accessLocalCost(l_routing_data, borderId2);
    } while(advanceThread());
  }
}

///




  void GPURouting::updateRouteMap()
  {
    glFinish();
    int computeAll = false;
    
    //update / create buffers
    if(_buffer_width != _subscribe_costmap->_data->width ||
       _buffer_height != _subscribe_costmap->_data->height)
    {
      _buffer_width = _subscribe_costmap->_data->width;
      _buffer_height = _subscribe_costmap->_data->height;

      _cl_lastCostMap_buffer = cl::Buffer(_cl_context, CL_MEM_READ_WRITE, _buffer_width*_buffer_height*sizeof(cl_float));
      _blocks[0] = divup<size_t>(_buffer_width, _blockSize[0]-1);
      _blocks[1] = divup<size_t>(_buffer_height, _blockSize[1]-1);
      
      int boundaryElements = 2*(_blockSize[0] + _blockSize[1]-2);

      _cl_routeMap_buffer =  cl::Buffer(_cl_context, CL_MEM_READ_WRITE, _blocks[0]*_blocks[1]*boundaryElements*boundaryElements/2*sizeof(cl_float));
      computeAll = true;
    }


    ////dummy call
    //float* costmap = new float[_buffer_width*_buffer_height];
    //float* lastcostmap = new float[_buffer_width*_buffer_height];
    //float* routemap = new float[_blocks[0]*_blocks[1]*2*(_blockSize[0] + _blockSize[1])*2*(_blockSize[0] + _blockSize[1])/2];
    //float* lmem = new float[2*(_blockSize[0]+2)*(_blockSize[1]+2)];
    //initKernelData(int2(_blockSize[0],_blockSize[1]), int2(_blocks[0], _blocks[1]));
    //costmap_dim = int2(_buffer_width, _buffer_height);

    //updateRouteMapDummy(costmap, costmap, routemap, int2(_buffer_width, _buffer_height), computeAll, lmem);
    //delete[] costmap;
    //delete[] lastcostmap;
    //delete[] routemap;
    //delete[] lmem;

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

    _cl_command_queue.enqueueAcquireGLObjects(&memory_gl);

    cl_int bufferDim[2] = { _buffer_width, _buffer_height };


    //update route map
    //_cl_updateRouteMap_kernel

    _cl_updateRouteMap_kernel.setArg(0, memory_gl[0]);
    _cl_updateRouteMap_kernel.setArg(1, _cl_lastCostMap_buffer);
    _cl_updateRouteMap_kernel.setArg(2, _cl_routeMap_buffer);
    _cl_updateRouteMap_kernel.setArg(3, 2 * sizeof(cl_int), bufferDim);
    _cl_updateRouteMap_kernel.setArg(4, computeAll);
    _cl_updateRouteMap_kernel.setArg(5, 2*(_blockSize[0]+2)*(_blockSize[1]+2)*sizeof(float), NULL);


    cl::Event updateRouteMap_Event;
    _cl_command_queue.enqueueNDRangeKernel
    (
      _cl_updateRouteMap_kernel,
      cl::NullRange,
      cl::NDRange(_blocks[0]*_blockSize[0], _blocks[1]*_blockSize[1]),
      cl::NDRange(_blockSize[0], _blockSize[1]),
      0,
      &updateRouteMap_Event
    );

    _cl_command_queue.enqueueReleaseGLObjects(&memory_gl);
    _cl_command_queue.finish();

    cl_ulong start, end;
    std::cout << "CLInfo:\n";
    updateRouteMap_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    updateRouteMap_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    std::cout << " - updateRouteMap: " << (end-start)/1000000.0  << "ms\n";
  }
  void GPURouting::createRoutes(LinksRouting::LinkDescription::HyperEdge&)
  {
    //bottom up routing -> for every level do:

    // _cl_prepareBorderCostsX_kernel

    // _cl_prepareBorderCostsY_kernel

    // _cl_prepareIndividualRouting_kernel

    // _cl_runIndividualRoutings_kernel

    // _cl_fillUpIndividualRoutings_kernel;
    // _cl_getMinimum_kernel;
    // _cl_routeInOut_kernel;
    // _cl_routeInterBlock_kernel;
    // _cl_routeConstruct_kernel;
  }

}
