#include "gpurouting.h"
#include "log.hpp"

#include <iostream>
#include <fstream>
#include <cmath>
#include <set>
#include <algorithm>
#include <limits>
#include <sstream>

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
    registerArg("QueueSize", _routingQueueSize = 128);
    registerArg("NumLocalWorkers", _routingNumLocalWorkers = 4);
    registerArg("WorkersWarpSize", _routingLocalWorkersWarpSize = 32);
    registerArg("BValue",_Bvalue = 1.0);
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

  //----------------------------------------------------------------------------
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
  bool GPURouting::initGL()
  {
    try
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
           if( dextensions.find("cl_khr_gl_sharing") != std::string::npos )
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

      _cl_initMem_kernel = cl::Kernel(_cl_program, "initMem");

      _cl_updateRouteMap_kernel = cl::Kernel(_cl_program, "updateRouteMap");
      _cl_prepareBorderCosts_kernel = cl::Kernel(_cl_program, "prepareBorderCosts");
      _cl_prepareIndividualRouting_kernel = cl::Kernel(_cl_program, "prepareIndividualRouting");
      _cl_prepareIndividualRoutingParent_kernel = cl::Kernel(_cl_program, "prepareIndividualRoutingParent");
      _cl_routing_kernel  = cl::Kernel(_cl_program, "routeLocal");


      _cl_voteMinimum_kernel = cl::Kernel(_cl_program, "voteMinimum");
      _cl_getMinimum_kernel = cl::Kernel(_cl_program, "getMinimum");
      _cl_routeInterBlock_kernel = cl::Kernel(_cl_program, "calcInterBlockRoute");
      _cl_routeConstruct_kernel = cl::Kernel(_cl_program, "routeConstruct");
    }
    catch(std::exception& ex)
    {
      std::cerr << "Failed to initialize GPURouting: "
                << ex.what()
                << std::endl;
      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  void GPURouting::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  template<typename T>
  void printfBuffer( cl::CommandQueue& cl_queue,
                     const cl::Buffer& buf,
                     size_t width,
                     size_t height,
                     const char* name = 0)
  {
    cl_queue.finish();

    std::vector<T> mem(width * height);
    cl_queue.enqueueReadBuffer(buf, true, 0, width * height * sizeof(T), &mem[0]);

    if(name)
      std::cout << name << ":\n";
    else
      std::cout << "Buffer:\n";
    for(auto it = mem.begin(); it != mem.end();)
    {
      for(size_t i = 0; i < width; ++i, ++it)
        std::cout << *it << " ";
      std::cout << '\n';
    }

  }
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
  void dumpImage( cl::CommandQueue& cl_queue,
                   const cl::Image& img,
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
    cl::size_t<3> o, r;
    o[0] = o[1] = o[2] = 0;
    r[0] = width; r[1] = height; r[2] = 1;
    cl_queue.enqueueReadImage(img, true, o, r, width*sizeof(T), 0, &mem[0]);

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
  void GPURouting::process(unsigned int type)
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

    try
    {
    updateRouteMap();

    // now start analyzing the links

    LinkDescription::LinkList& links = *_subscribe_links->_data;
//    unsigned int width = _subscribe_costmap->_data->width,
//              height = _subscribe_costmap->_data->height,
//              num_points = width * height;
//    int dim[] = {width, height, num_points};
//    int downsample = _subscribe_desktop->_data->width / _subscribe_costmap->_data->width;
//    unsigned int numBlocks[2] = {divup<unsigned>(width,_blockSize[0]),divup<unsigned>(height,_blockSize[1])};
//    unsigned int sumBlocks = numBlocks[0]*numBlocks[1];

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
        if( node->get<bool>("hidden", false) )
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

        if( !_noQueue )
        {
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
        fork->position.x = (startingpoint[0] + 0.5) * downsample;
        fork->position.y = (startingpoint[1] + 0.5) * downsample;
        for(int i = 0; i < numtargets; ++i)
        {
          fork->outgoing.push_back(LinkDescription::HyperEdgeDescriptionSegment());
          fork->outgoing.back().parent = fork;
          fork->outgoing.back().trail.reserve(interblockinfo[i].s[1]);
          for(unsigned int j = 0; j < interblockinfo[i].s[1]; ++j)
          {
            fork->outgoing.back().trail.push_back
            (
              float2( (outroutes[i*maxpoints +j].s[0] + 0.5) * downsample,
                      (outroutes[i*maxpoints +j].s[1] + 0.5) * downsample )
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
  int2 operator *(const int a)
  {
    return int2(x*a, y*a);
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


float readLastPenalty(const float* lastCostmap, int2 pos, const int2 size)
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
        const float a = 1.0f; //a must be 1 !!!!
        const float b = 100.0f;
        *myval = std::min(*myval, r_other + a*d + b*d*penalty);
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
  bool changed;
  changed = computeAll>0?true:false;
  const float MAXFLOAT = std::numeric_limits<float>::max();

  float* l_cost = l_data;
  float* l_routing_data = l_data + (get_local_size(0)+2)*(get_local_size(1)+2);

  do
  {
//    int2 gid = int2(get_group_id(0)*(get_local_size(0)-1) + get_local_id(0), get_group_id(1)*(get_local_size(1)-1) + get_local_id(1));

    unsigned int lid = get_local_id(0) + get_local_id(1)*get_local_size(0);
    for(; lid < (get_local_size(0)+2)*(get_local_size(1)+2); lid += get_local_size(0)*get_local_size(1))
    {
      l_cost[lid] = 0.1f*MAXFLOAT;
      l_routing_data[lid] = 0.1f*MAXFLOAT;
    }
  } while(advanceThread());
  do
  {
    float last_cost = 0.1f*MAXFLOAT;
    int2 gid = int2(get_group_id(0)*(get_local_size(0)-1) + get_local_id(0), get_group_id(1)*(get_local_size(1)-1) + get_local_id(1));
    if(gid.x < dim.x && gid.y < dim.y)
    {
      float newcost = getPenalty(costmap, gid);
      last_cost = readLastPenalty(lastCostmap, gid, dim);
      *accessLocalCost(l_cost, int2(get_local_id(0), get_local_id(1))) = newcost;
      if(std::abs(last_cost - newcost) > 0.01f )
      {
        changed = true;
        writeLastPenalty(lastCostmap, gid, dim, newcost);
      }
    }
  } while(advanceThread());

  if(!changed) return;


  //do the routing
  int boundaryelements = 2*(get_local_size(0) + get_local_size(1)-2);
  int written = 0;
  for(int i = 0; i < boundaryelements; ++i)
  {
    do
    {
      int myBorderId = borderId(int2(get_local_id(0), get_local_id(1)), int2(get_local_size(0),get_local_size(1)));
      //init routing data
      float mydata = 0.1f*MAXFLOAT;
      if(i == myBorderId)
        mydata = 0.0f;
      *accessLocalCost(l_routing_data, int2(get_local_id(0), get_local_id(1))) = mydata;
    } while(advanceThread());
    //route
    route_data(l_cost, l_routing_data);

    do
    {
      int myBorderId = borderId(int2(get_local_id(0), get_local_id(1)), int2(get_local_size(0),get_local_size(1)));
      //write result
      if(myBorderId >= i)
        routeMap[boundaryelements*boundaryelements/2*(get_group_id(0)+get_group_id(1)*get_num_groups(0)) +
                 written + myBorderId - i] = *accessLocalCost(l_routing_data, int2(get_local_id(0), get_local_id(1)));
    } while(advanceThread());
    written += boundaryelements - i;
  }
}


int getCostGlobalId(const int lid, const int2 blockId, const int2 blockSize, int2 blocks)
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



  //blocks.x = (blocks.x & ~0x1)+1;
  //blocks.y = (blocks.y & ~0x1)+1;
  //int borderElements = 2*(blockSize.x + blockSize.y - 2);
  //int full_row_offset = blocks.x*(blockSize.y+2*blockSize.x-4) + blockSize.y;
  //int half_row_offset = (blocks.x+1)*(blockSize.y-2);
  //int myoffset = blockId.y/2*(full_row_offset+half_row_offset);
  //int mycoloffset = blockId.x/2*(borderElements + 2*blockSize.x - 4);
  //if((blockId.x & 0x1) == 0 && (blockId.y & 0x1) == 0)
  //{
  //  //full mode
  //  //can use lid directly
  //  return myoffset + mycoloffset + lid;
  //}
  //if((blockId.y & 0x1) == 0)
  //{
  //  //between two full (left and right)
  //  myoffset = myoffset + mycoloffset;
  //  if(lid == 0)
  //    return myoffset + blockSize.x - 1;
  //  if(lid < blockSize.x -1)
  //    return myoffset + borderElements + lid - 1;
  //  if(lid == blockSize.x - 1)
  //    return myoffset + borderElements + 2*blockSize.x - 4;
  //  if(lid < blockSize.x + blockSize.y - 1)
  //    return myoffset + 2*(borderElements + blockSize.x - 2) + blockSize.x - 1 - lid;
  //  if(lid < 2*blockSize.x + blockSize.y - 3)
  //    return myoffset + borderElements + lid - 1 - blockSize.y;
  //  else
  //    return myoffset + borderElements - lid + blockSize.x - 1;
  //}
  //if((blockId.x &0x1) == 0)
  //{
  //  //between two full (top and and)
  //  int halfcoloffset = blockId.x/2*(2*blockSize.y - 2);
  //  if(lid < blockSize.x)
  //    return myoffset + mycoloffset + 2*blockSize.x + blockSize.y - 3 - lid;
  //  if(lid < blockSize.x + blockSize.y - 2)
  //    return myoffset + full_row_offset + halfcoloffset + lid - blockSize.x;
  //  if(lid < 2*blockSize.x + blockSize.y - 2)
  //    return myoffset + full_row_offset+half_row_offset + mycoloffset + 2*blockSize.x + blockSize.y-3 -lid;
  //  else
  //    return myoffset + full_row_offset + halfcoloffset + lid - 2*blockSize.x;
  //}
  //else
  //{
  //  //full partial
  //  if(lid == 0)
  //    return myoffset + mycoloffset + blockSize.x + blockSize.y - 2;
  //  if(lid < blockSize.x-1)
  //    return myoffset + mycoloffset + borderElements + 2*(blockSize.x-2) - lid;
  //  if(lid == blockSize.x-1)
  //    return myoffset + mycoloffset + (borderElements + 2*blockSize.x - 2) + 2*blockSize.x + blockSize.y - 3;
  //  if(lid < blockSize.x + blockSize.y - 2)
  //    return myoffset + full_row_offset + (blockId.x+1)*(blockSize.y - 2) + 2*(blockSize.y-2)-1 -(lid-blockSize.x);
  //  if(lid == blockSize.x + blockSize.y - 2)
  //    return myoffset + full_row_offset+half_row_offset + mycoloffset + (borderElements + 2*blockSize.x - 4) + 0;
  //  if(lid < 2*blockSize.x + blockSize.y - 3)
  //    return myoffset + full_row_offset+half_row_offset + mycoloffset + borderElements + 2*blockSize.x+blockSize.y-4 -lid;
  //  if(lid == 2*blockSize.x + blockSize.y - 3)
  //    return myoffset + full_row_offset+half_row_offset + mycoloffset + blockSize.x-1;
  //  else
  //    return myoffset + full_row_offset + blockId.x/2*(2*blockSize.y - 4) + borderElements-1 -lid;
  //}
}

typedef unsigned int uint;
float getRouteMapCost(const float* ourRouteMapBlock, int from, int to, const int borderElements)
{
  int row = std::min(from,to);
  int col = std::max(from,to);
  col = col - row;
  return ourRouteMapBlock[col + row*borderElements - row*(row-1)/2];
}
int dir4FromBorderId(int borderId, int2 lsize)
{
  if(borderId < 0) return 0;
  int mask =  (borderId < lsize.x) |
              ((borderId >= lsize.x-1 && borderId < lsize.x+lsize.y-1 ) << 1) |
              ((borderId >= lsize.x+lsize.y-2 && borderId < 2*lsize.x+lsize.y-2 ) << 2) |
              ((borderId >= 2*lsize.x+lsize.y-3 || borderId == 0) << 3);
  return mask;
}
int routeBorder(std::vector<float>& routeMap,
                float* routingData,
                const int2 blockId,
                const int2 blockSize,
                const int2 numBlocks,
                 float* routeData,
                 int* changeData)
{
//  int lsize = get_local_size(0);
  int borderElements = 2*(blockSize.x + blockSize.y - 2);
  const float MAXFLOAT = std::numeric_limits<float>::max();

  *changeData = 0;

  const float* ourRouteMap = &routeMap[0] + borderElements*(borderElements+1)/2*(blockId.x + blockId.y*numBlocks.x);
  do
  {
    float mycost = 0.1f*MAXFLOAT;
    int lid = get_local_id(0);
    int gid = getCostGlobalId(lid, blockId, blockSize, numBlocks);
    if(lid < borderElements)
    {
      mycost = routingData[gid];
      routeData[lid] = mycost;
    }
  }while(advanceThread());

  do
  {
    int lid = get_local_id(0);
    int gid = getCostGlobalId(lid, blockId, blockSize, numBlocks);
  if(lid < borderElements)
  {

    float mycost = 0.1f*MAXFLOAT;
    if(lid < borderElements)
      mycost = routingData[gid];

    float origCost = mycost;
    for(int i = 0; i < borderElements; ++i)
    {
      float newcost = routeData[i] + getRouteMapCost(ourRouteMap, i, lid, borderElements);
      mycost = std::min(newcost, mycost);
    }

    if(mycost + 0.0001f < origCost)
    {
      //atomic as multiple might be working on the same
      routingData[gid] =std::min(routingData[gid] , mycost);
      routeData[lid] = mycost;

      //TODO: this is already an extension for opencl 1.0 so maybe do them individually...
      *changeData |= dir4FromBorderId(lid, blockSize);
    }
  }
  } while(advanceThread());
  return *changeData;
}

void nextBlockRange(uint* dims, uint* limits)
{
  uint maxuint = 0xFFFFFFFF;
  uint current =  maxuint - std::max(std::max(dims[0], dims[1]),std::max(dims[2], dims[3])) + 1;
  //get next highest
  uint next = std::max(std::max(dims[0]+current,dims[1]+current),std::max(dims[2]+current,dims[3]+current));
  next = (next != 0)*(next - current);
  for(int i = 0; i < 4; ++i)
    dims[i] = std::min(limits[i],next);
}
int2 activeBlockToId(const int2 seedBlock, const int spiralIdIn, const int2 activeBlocksSize)
{
  int spiralId = spiralIdIn/2;
  //get limits
  int2 seedAB = int2(seedBlock.x/8, seedBlock.y/8);
  uint4 spiral_limits = uint4( seedAB.x+1,  seedAB.y+1,
    activeBlocksSize.x - seedAB.x, activeBlocksSize.y - seedAB.y);
  uint4 area_dims = spiral_limits;

  int2 resId = seedAB;
  for(int i = 0; i < 4; ++i)
  {
    nextBlockRange(&area_dims.x, &spiral_limits.x);
    int w = std::max(1, (int)(area_dims.x + area_dims.z - 1));
    int h = std::max(1, (int)(area_dims.y + area_dims.w - 1));
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

      int innerspirals = std::max(1U,std::max(std::max(area_dims.x, area_dims.y),std::max(area_dims.z, area_dims.w)));
      int4 allspirals = int4(std::max(0,seedAB.x-innerspirals-outerspirals+1), std::max(0,seedAB.y-innerspirals-outerspirals+1),
                             std::min(activeBlocksSize.x-1,seedAB.x+innerspirals+outerspirals-1), std::min(activeBlocksSize.y-1,seedAB.y+innerspirals+outerspirals-1));
      int allspiralelements = (allspirals.z - allspirals.x+1)*(allspirals.w - allspirals.y+1);
      int myoffset = spiralId - allspiralelements;

      //find the right position depend on the offset
      if(allspirals.y > 0)
      {
        int topelements = allspirals.z - allspirals.x + 1 + (allspirals.z < activeBlocksSize.x-1);
        if(myoffset < topelements)
        {
          resId = int2(allspirals.x + myoffset, allspirals.y-1);
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
          resId = int2(allspirals.z + 1, allspirals.y + myoffset);
          break;
        }
        else
          myoffset -= rightelements;
      }
      if(allspirals.w < activeBlocksSize.y-1)
      {
        int bottomelements = allspirals.z - allspirals.x + 2;
        if(myoffset < bottomelements)
        {
          resId = int2(allspirals.z - myoffset, allspirals.w + 1);
          break;
        }
        else
          myoffset -= bottomelements;
      }
      //else
      resId = int2(allspirals.x - 1, allspirals.w - myoffset);
      break;
    }
  }

  return int2(resId.x*8, resId.y*8 + (spiralIdIn&1)*4);

}

int2 getActiveBlock(const int2 seedBlock, const int2 blockId, const int2 activeBlocksSize)
{
  int2 seedAB = int2(seedBlock.x/8, seedBlock.y/8);
  int2 blockAB = int2(blockId.x/8, blockId.y/8);
  //get full spiral count
  int spirals = std::max(std::max(seedAB.x - blockAB.x, blockAB.x - seedAB.x), std::max(seedAB.y - blockAB.y, blockAB.y - seedAB.y));

  uint offset = 0;
  int2 diff = blockAB - seedAB;

  //top row
  if(diff.y == -spirals && diff.x > -spirals)
  {
    int4 dims;
    dims.x = std::max(0, seedAB.x - spirals + 1);
    dims.z = std::min(activeBlocksSize.x - 1, seedAB.x + spirals - 1);
    dims.y = seedAB.y - spirals + 1;
    dims.w = std::min(activeBlocksSize.y - 1, seedAB.y + spirals - 1);
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      blockAB.x - dims.x;
  }
  //right side
  else if(diff.x == spirals)
  {
    int4 dims;
    dims.x = std::max(0, seedAB.x - spirals + 1);
    dims.z = seedAB.x + spirals - 1;
    dims.y = std::max(0, seedAB.y - spirals);
    dims.w = std::min(activeBlocksSize.y - 1, seedAB.y + spirals - 1);
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      blockAB.y - dims.y;
  }
  //bottom side
  else if(diff.y == spirals)
  {
    int4 dims;
    dims.x = std::max(0, seedAB.x - spirals + 1);
    dims.z = std::min(activeBlocksSize.x - 1, seedAB.x + spirals);
    dims.y = std::max(0, seedAB.y - spirals);
    dims.w = seedAB.y + spirals - 1;
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      dims.z - blockAB.x;
  }
  //left side
  else
  {
    int4 dims;
    dims.x = seedAB.x - spirals + 1;
    dims.z = std::min(activeBlocksSize.x - 1, seedAB.x + spirals);
    dims.y = std::max(0, seedAB.y - spirals);
    dims.w = std::min(activeBlocksSize.y - 1, seedAB.y + spirals);
    offset = (dims.z - dims.x + 1) * (dims.w - dims.y + 1) +
      dims.w - blockAB.y;
  }

  int2 interBlock = int2(blockId.x%8, blockId.y%8);
  int second = interBlock.y >= 4;
  interBlock.y -= second*4;
  uint mask = 1u << (uint)(interBlock.x + interBlock.y*8);
  return int2(2*offset + second, mask);
}

void unsetActiveBlock(uint* myActiveBlock, const int2 seedBlock, const int2 blockId, const int2 activeBlocksSize)
{
  int2 tblock = getActiveBlock(seedBlock, blockId, activeBlocksSize);
  myActiveBlock[tblock.x] &= (~tblock.y);
}
void setActiveBlock( uint* myActiveBlock, const int2 seedBlock, const int2 blockId, const int2 activeBlocksSize)
{
  int2 tblock = getActiveBlock(seedBlock, blockId, activeBlocksSize);
  myActiveBlock[tblock.x] |= tblock.y;
}

uint createDummyQueueEntry()
{
  return 0xFFFFFFFF;
}
uint createQueueEntry(const int2 block, const float priority, const uint minPriorty = 1u)
{
  return ((std::max(minPriorty,std::min((uint)(priority*0.5f),0xFFFu))) << 20) |
         (block.y << 10) | block.x;
}
int2 readQueueEntry(const uint entry)
{
  return int2(entry & 0x3FFu, (entry >> 10) & 0x3FFu);
}
bool compareQueueEntryBlocks(const uint entry1, const uint entry2)
{
  return (entry1&0xFFFFFu)==(entry2&0xFFFFFu);
}
uint popc(uint val)
{
  uint c = 0;
  for(uint i = 0; i < 32; ++i, val >>= 1)
    c += val & 0x1;
  return c;
}
uint scanLocalWorkEfficient(uint* data, size_t linId, size_t elements)
{
  uint v = 0;
  for(size_t i = 0; i < elements; ++i)
  {
    uint t = v;
    v += data[i];
    data[i] = t;
  }
  return data[elements] = v;

}
#if ROUTING_DEBUG
void routeLocal( std::vector<float>& routeMap,
                 std::vector<int4>& seed,
                 std::vector<cl_uint>& routingIds,
                 std::vector<float>& routingData_in,
                         const int routeDataPerNode,
                         const int2 blockSize,
                         const int2 numBlocks,
                         int* data,
                         uint* queue,
                         const int queueSize,
                         uint* activeBuffer,
                         const int2 activeBufferSize)
{
  std::vector<uint> changed_data(get_local_size(0)*get_local_size(1));
  int borderElements = 2*(blockSize.x + blockSize.y - 2);
  //int workerId = get_local_id(1);
  //const int lid = get_local_id(0);
  //const int linId = get_local_id(0) + get_local_size(0)*get_local_id(1);
  const int workerSize = get_local_size(0);
  const uint workers = get_local_size(1);
  //int withinWorkerId = get_local_id(0);
  int elementsPerWorker = borderElements + 1;
  float* routingData = &routingData_in[0] + routeDataPerNode*routingIds[get_group_id(0)];

  int2 seedBlock;
  std::vector<int> routed(numBlocks.x*numBlocks.y, 0);
  std::set<uint> activeBufferCheck;

   bool thisRunChange;
  //do

    thisRunChange = false;
    //(re)init queue
    //TODO: this needs to circle around until we find a block which has changed
    int4 ourInit = seed[get_group_id(0)];
    seedBlock = int2((ourInit.x+ourInit.z)/2, (ourInit.y+ourInit.w)/2);
    uint queueFront = 0;
    uint queueElements = (ourInit.w-ourInit.y)*(ourInit.z-ourInit.x);
    do
    {
    int posoffset = (ourInit.z-ourInit.x)*get_local_id(1);
    for(int y = ourInit.y + get_local_id(1); y < ourInit.w; y+= get_local_size(1))
    {
      for(int x = get_local_id(0); x < (ourInit.z - ourInit.x); x += get_local_size(0))
      {
        int pos = posoffset + x;
        queue[pos] = createQueueEntry(int2(ourInit.x+x, y), 0, 0);
      }
      posoffset += (ourInit.z-ourInit.x);
    }
    } while(advanceThread());

    while(queueElements)
    {
    //do the work
    while(queueElements)
    {
      int changed = 0;
      int2 blockId;

      //todo iterate over workers
      unsigned int workerId = get_local_id(1);
      float* l_routeData = ( float*)(data + elementsPerWorker*workerId);
      int* l_changedData = ( int*)(l_routeData + borderElements);
      if(queueElements > workerId)
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
        unsetActiveBlock(activeBuffer + 2*activeBufferSize.x*activeBufferSize.y*get_group_id(0), seedBlock, blockId, activeBufferSize);
        activeBufferCheck.erase((blockId.y << 16) | blockId.x);
        ++routed[blockId.x + blockId.y*numBlocks.x];
        do {
          changed_data[get_local_id(0) + get_local_size(0)*get_local_id(1)] = changed;
        } while(advanceThread());
        if(changed) thisRunChange = true;
      }
      uint nowWorked = std::min(queueElements, workers);
      queueFront += nowWorked;
      queueElements -= nowWorked;

      //queue management (one after the other)
      uint l_queueElement;
      uint hasElements;
      for(uint w = 0; w < nowWorked; ++w)
      {
        do
        {
          uint workerId = get_local_id(1);
          changed = changed_data[get_local_id(0) + get_local_size(0)*get_local_id(1)];
          if(w == workerId)
            hasElements = changed;
        } while(advanceThread());

        for(int element = 0; element < 4; ++element)
        {
          l_queueElement = 0xFFFFFFFF;
          if((hasElements & (1 << element)) == 0)
            continue;
          //generate new entry
          do
          {
            uint workerId = get_local_id(1);
            uint lid = get_local_id(0);
            if(w == workerId && ((1 << element) & dir4FromBorderId(lid,blockSize)))
            {
              int2 offset = int2(element>0?2-element:0, element<3?element-1:0);
              int2 newId = blockId + offset;
              if(newId.x >= 0 && newId.y >= 0 &&
                newId.x < numBlocks.x && newId.y < numBlocks.y)
                l_queueElement = std::min(l_queueElement,createQueueEntry(newId, l_routeData[lid]));
            }
          } while(advanceThread());

          if(l_queueElement == 0xFFFFFFFF)
            continue;

          --queueFront;
          queueFront = (queueFront + queueSize) % queueSize;

          //insert new entry
          if(queueElements == queueSize)
          {
            queueElements = queueSize-1;
            //std::cout << "putting in: " << readQueueEntry(queue[queueFront]).x << ", " << readQueueEntry(queue[queueFront]).y << std::endl;
            activeBufferCheck.insert((readQueueEntry(queue[queueFront]).y << 16) | readQueueEntry(queue[queueFront]).x);
            setActiveBlock(activeBuffer + 2*activeBufferSize.x*activeBufferSize.y*get_group_id(0), seedBlock, readQueueEntry(queue[queueFront]), activeBufferSize);
          }


          uint newEntry = l_queueElement;

          l_queueElement = queueSize;
          queue[queueFront] = newEntry;

          for(int i=0; i < queueElements; i+=get_local_size(0)*get_local_size(1))
          {
            do
            {
              const int linId = get_local_id(0) + get_local_size(0)*get_local_id(1);
              int tId = linId + i;
              uint nextEntry;
              if(tId < queueElements)
              {
                nextEntry = queue[(queueFront + tId + 1)%queueSize];
                //did we find the entry?
                if(compareQueueEntryBlocks(nextEntry, newEntry))
                  l_queueElement = tId;
              }
            } while(advanceThread());

            do
            {
              const int linId = get_local_id(0) + get_local_size(0)*get_local_id(1);
              int tId = linId + i;
              if(tId < queueElements)
              {
                uint nextEntry = queue[(queueFront + tId + 1)%queueSize];
                //if new element has higher cost -> copy old back
                //if same element has been found -> need to copy all back
                if(nextEntry < newEntry || tId > l_queueElement)
                  queue[(queueFront + tId)%queueSize] = nextEntry;
                //insert the new element, if my old one is the last to be copied away
                else if(queue[(queueFront + tId)%queueSize] < newEntry)
                  queue[(queueFront + tId)%queueSize] = newEntry;
              }
            } while(advanceThread());
          }

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


    queueFront = 0;
    uint pullers = std::min(queueSize/2, (int)(get_local_size(0)*get_local_size(1)) );
    int pullRun = 0;
    while(queueElements < queueSize/4 && pullRun * pullers < 2*activeBufferSize.x*activeBufferSize.y)
    {
      uint myentry = 0;
      uint canOffer = 0;
      do
      {
        int linId = get_local_id(0);
        int pullId = linId + pullRun*pullers;
        if(linId < pullers)
        {
          if(pullId < 2*activeBufferSize.x*activeBufferSize.y)
          {
            myentry = activeBuffer[2*activeBufferSize.x*activeBufferSize.y*get_group_id(0) + pullId];
            canOffer = popc(myentry);
          }
          queue[queueSize/2 + linId-1] = canOffer;
        }
      }while(advanceThread());

      //run prefix sum
      scanLocalWorkEfficient(queue + queueSize/2-1, 0, pullers);

      //put into queue
      do
      {
        int linId = get_local_id(0);
        int pullId = linId + pullRun*pullers;
        if(linId < pullers)
        {
          if(pullId < 2*activeBufferSize.x*activeBufferSize.y)
          {
            myentry = activeBuffer[2*activeBufferSize.x*activeBufferSize.y*get_group_id(0) + pullId];
            canOffer = popc(myentry);


            int myElements = std::min((int)canOffer, queueSize/4 - (int)(queueElements + queue[queueSize/2-1 + linId]));
            int2 myOffset = activeBlockToId(seedBlock, pullId, activeBufferSize);

            int2 additionalOffset = int2(0,0);
            for(int inserted = 0; additionalOffset.y < 4; ++additionalOffset.y)
              for(additionalOffset.x = 0; additionalOffset.x < 8 && inserted < myElements; ++additionalOffset.x, myentry >>= 1)
              {
                if(myentry & 0x1)
                {
                  //std::cout << "taking out: " << (myOffset + additionalOffset).x << ", " << (myOffset + additionalOffset).y << std::endl;

                  auto found = activeBufferCheck.find( ((myOffset + additionalOffset).y << 16) |  (myOffset + additionalOffset).x);
                  if(found == activeBufferCheck.end())
                    std::cout << "oh no!\n";
                  else
                    activeBufferCheck.erase(found);
                  queue[queueElements + queue[queueSize/2-1 + linId] + inserted] = createQueueEntry(myOffset + additionalOffset , 0, 0);
                  ++inserted;
                }
              }
          }
        }
      } while(advanceThread());
      queueElements =  std::min((int)(queueElements + queue[queueSize/2 + pullers-1]), queueSize/4);

      pullRun++;
    }


  }

  float count = 0;
  for(auto it = routed.begin(); it != routed.end(); ++it)
    if(*it == 0)
      std::cout << "ouch!!!\n";
    else count += *it;
    std::cout << "avg iterations per block: " << (count/routed.size()) << "\n";

}

void validateRouteMap(const float* routeMap, size_t borderElements)
{
  for(size_t from = 0; from < borderElements; ++from)
  {
    for(size_t to = 0; to < borderElements; ++to)
    {
      //float a = getRouteMapCost(routeMap, from, to, borderElements);
      //if(a != std::max(from, to) && (from != to || a != 0))
      //  std::cout << "outch\n";
      float directCost = getRouteMapCost(routeMap, from, to, borderElements);
      for(size_t over = 0; over < borderElements; ++over)
      {
        float overcost0 = getRouteMapCost(routeMap, from, over, borderElements);
        float overcost1 = getRouteMapCost(routeMap, over, to, borderElements);
        float overcost = overcost0 + overcost1;
        if(overcost + 0.001f < directCost)
          std::cout << "routeMap not valid: direct cost (" << from << "->" << to << "): " << directCost << " > (" << from << "->" << over << "->" << to << "): " << overcost << "\n";
      }
    }
  }
}

void prepareIndividualRoutingDummy(const float* costmap,
                                    float* routedata,
                                   std::vector<int4>& startingPoints,
                                   int4* blockToDataMapping,
                                       const int2 dim,
                                       const int2 numBlocks,
                                       const int routeDataPerNode,
                                       float* l_data)
{
  int4 mapping = blockToDataMapping[get_group_id(0)];




  const float MAXFLOAT = std::numeric_limits<float>::max();

  float* l_cost = l_data;
  float* l_route = l_data + (get_local_size(0)+2)*(get_local_size(1)+2);


  //loadCostToLocalAndSetRoute((int2)(mapping.x, mapping.y), gid, dim, costmap, l_cost, l_route);
  do
  {
    uint lid = get_local_id(0) + get_local_id(1)*get_local_size(0);
    for(; lid < (get_local_size(0)+2)*(get_local_size(1)+2); lid += get_local_size(0)*get_local_size(1))
    {
      l_cost[lid] = 0.1f*MAXFLOAT;
      l_route[lid] = 0.1f*MAXFLOAT;
    }
  }while(advanceThread());

  do
  {
    float r_in = 0.1f*MAXFLOAT;
    int2 gid = int2(mapping.x*(get_local_size(0)-1)+get_local_id(0),
                    mapping.y*(get_local_size(1)-1)+get_local_id(1));
    if(gid.x < dim.x && gid.y < dim.y)
    {
      r_in = readLastPenalty(&costmap[0], gid, dim);
      *accessLocalCost(l_cost, int2(get_local_id(0), get_local_id(1))) = r_in;
    }
  }while(advanceThread());

  do
  {
    int2 gid = int2(mapping.x*(get_local_size(0)-1)+get_local_id(0),
                    mapping.y*(get_local_size(1)-1)+get_local_id(1));
    float startcost = 0.1f*MAXFLOAT;
    if(gid.x < dim.x && gid.y < dim.y)
    {
      if(startingPoints[mapping.z].x <= gid.x && gid.x <= startingPoints[mapping.z].z &&
         startingPoints[mapping.z].y <= gid.y && gid.y <= startingPoints[mapping.z].w)
          startcost = 0;
    }
    *accessLocalCost(l_route, int2(get_local_id(0),get_local_id(1))) = startcost;
  } while(advanceThread());


  //route
  route_data(l_cost, l_route);

  do
  {
    int lid =  borderId(int2(get_local_id(0),get_local_id(1)), int2(get_local_size(0), get_local_size(1)));
    if(lid >= 0)
    {
      //write out result
      int offset = getCostGlobalId(lid, int2(mapping.x, mapping.y), int2 (get_local_size(0), get_local_size(1)), numBlocks);
      routedata[routeDataPerNode*mapping.w + offset] = *accessLocalCost(l_route, int2(get_local_id(0),get_local_id(1)));
    }
  } while(advanceThread());
}


int dir8FromBorderId(int borderId, int2 lsize)
{
  int borderElements = 2*(lsize.x+lsize.y-2);
  if(borderId < 0 || borderId >= borderElements) return 0;
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

int2 getOffsetFromDir8Element(int element)
{
  int2 offset = int2(0,0);
  if(element == 0 || element > 5) offset.x = -1;
  else if(element > 1 && element < 5) offset.x = 1;
  if(element < 3) offset.y = -1;
  else if(element > 3 && element < 7) offset.y = 1;
  return offset;
}

int writeOutBlock(uint* interblockResult, const int interblockSize, const int layer, const int2 blockId, const int2 blockSize, const int2 numBlocks, const int2 enter, const int written)
{
  //ran out of space :/
  if(written + 3 >= interblockSize)
    return written;
  interblockResult[interblockSize*layer] = (written + 2)/2;
  interblockResult = interblockResult + interblockSize*layer + written;

  uint blockEncode = blockId.x + blockId.y*numBlocks.x;
  interblockResult[1] = blockEncode;

  int innerBlockEncode = (enter.x - blockId.x*(blockSize.x-1)) + (enter.y - blockId.y*(blockSize.y-1))*blockSize.x;
  interblockResult[2] = innerBlockEncode;

  return written+2;
}

bool isContainedInBlock(const int4 target, const int2 block, const int2 blockSize)
{
  int4 blockBounds = int4(block.x * (blockSize.x -1), block.y * (blockSize.y -1),
                            (block.x+1) * (blockSize.x -1), (block.y+1) * (blockSize.y -1));
  bool xoverlap =  target.x <= blockBounds.z && target.z >= blockBounds.x;
  bool yoverlap =  target.y <= blockBounds.w && target.w >= blockBounds.y;
  return xoverlap && yoverlap;
}

float2 checkRoutes(int2 blockId, int startLid, const float* routecost, const float* routeMap, int layer, int2 numBlocks, int2 blockSize, int routeDataPerNode)
{
  const float MAXFLOAT = std::numeric_limits<float>::max();
  float minresX, minresY;
  minresX = 0.1f*MAXFLOAT;
  minresY = 0.1f*MAXFLOAT;

  std::vector<float2> newmincosts(get_local_size(0)*get_local_size(1), float2(0.1f*MAXFLOAT,  0.1f*MAXFLOAT));

  do
  {
    int linid = get_local_id(0) + get_local_id(1)*get_local_size(0);
    int lid =  borderId(int2(get_local_id(0),get_local_id(1)), blockSize);
    //avoid going back the same way
    if(lid >= 0 && lid != startLid)
    {

      int borderElements = 2*(blockSize.x + blockSize.y - 2);
      const float* ourRouteMap = routeMap + borderElements*(borderElements+1)/2*(blockId.x + blockId.y*numBlocks.x);
      int gid = getCostGlobalId(lid, blockId, blockSize, numBlocks);
      newmincosts[linid].x = routecost[routeDataPerNode*layer + gid];
      //increase value for close neighbours (we want to make progress and not jump between blocks)
      int neg_dist = (borderElements-std::min(abs(lid - startLid) , abs(abs(lid - startLid)-borderElements+2)));
      newmincosts[linid].y = 0.0001f*neg_dist + newmincosts[linid].x + getRouteMapCost(ourRouteMap, startLid, lid, borderElements);

      //TODO: reduction..
      minresY = std::min(minresY, newmincosts[linid].y);
    }
  }while(advanceThread());

  do
  {
  int linid = get_local_id(0) + get_local_id(1)*get_local_size(0);
  if(minresY == newmincosts[linid].y)
    minresX = newmincosts[linid].x;
  }while(advanceThread());

  return float2(minresX, minresY);
}

namespace CalcInterBlockRoute
{
  struct LocalData
  {
    int2 gid2;
    int2 lid2;
    int lid;
    int mydir;
    float myoutcost;
    int written;
    static int getOffset()
    {
      return get_local_id(0) + get_local_id(1)*get_local_size(0);
    }
  };
}


void calcInterBlockRouteDummy(const float* routecost,
                              const float* routeMap,
                              const float* costmap,
                              const uint4* froms,
                              const int4* tos,
                              const int* ids,
                              const int routeDataPerNode,
                              const int2 blockSize,
                              const int2 numBlocks,
                              const int2 dim,
                              uint* interblockResult,
                              const int interblockSize,
                              float* l_cost,
                              float* l_route)
{
  using namespace CalcInterBlockRoute;
  int layer = ids[0];
  int2 currentBlock = int2(froms[layer].x/(blockSize.x-1), froms[layer].y/(blockSize.y-1));
  const float MAXFLOAT = std::numeric_limits<float>::max();


  std::vector<LocalData> locals(get_local_size(0)*get_local_size(1));

  do
  {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    mylocal.lid2 = int2(get_local_id(0),get_local_id(1));
    mylocal.lid =  borderId(mylocal.lid2, blockSize);
    mylocal.mydir = dir8FromBorderId(mylocal.lid,blockSize);
    mylocal.written = 0;
    if(mylocal.lid == 0)
      mylocal.written = writeOutBlock(interblockResult, interblockSize, layer, currentBlock, blockSize, numBlocks, int2(froms[layer].x,froms[layer].y) , mylocal.written);
  } while(advanceThread());


  //early termination
  if(isContainedInBlock(tos[layer], currentBlock, blockSize))
    return;

  do
  {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    //find first out
    mylocal.myoutcost = 0.1f*MAXFLOAT;
    if(mylocal.lid >= 0)
    {
      int mydata_offset = getCostGlobalId(mylocal.lid, currentBlock, blockSize, numBlocks);
      mylocal.myoutcost = routecost[layer*routeDataPerNode +  mydata_offset];
    }
    mylocal.gid2 = int2(currentBlock.x * (blockSize.x-1) + get_local_id(0),
                       currentBlock.y * (blockSize.y-1) + get_local_id(1));
  } while(advanceThread());

  do
  {
    uint linid = get_local_id(0) + get_local_id(1)*get_local_size(0);
    for(; linid < (get_local_size(0)+2)*(get_local_size(1)+2); linid += get_local_size(0)*get_local_size(1))
    {
      l_cost[linid] = 0.1f*MAXFLOAT;
      l_route[linid] = 0.1f*MAXFLOAT;
    }
  } while(advanceThread());
  do
  {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    float r_in = 0.1f*MAXFLOAT;
    if(mylocal.gid2.x < dim.x && mylocal.gid2.y < dim.y)
    {
      r_in = readLastPenalty(costmap, mylocal.gid2, dim);
      *accessLocalCost(l_cost, int2(get_local_id(0), get_local_id(1))) = r_in;
    }
  } while(advanceThread());

  do
  {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    float myinit = 0.1f*MAXFLOAT;
    if(froms[layer].x == mylocal.gid2.x && froms[layer].y == mylocal.gid2.y)
      myinit = 0;
    *accessLocalCost(l_route, mylocal.lid2) = myinit;
  } while(advanceThread());


  //route
  route_data(l_cost, l_route);


  do
  {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    mylocal.myoutcost += *accessLocalCost(l_route, mylocal.lid2);
    //there will always be two elements with the same value if we start at the border, so choose the one which goes out
    if(mylocal.gid2.x == froms[layer].x && mylocal.gid2.y == froms[layer].y)
      mylocal.myoutcost += 0.0001f;
    //debug
    *accessLocalCost(l_route, mylocal.lid2) = mylocal.myoutcost;
  } while(advanceThread());

  //vote for minimal out cost
  float minvoter;
  float incost;
  int2 localInPos;

  incost = MAXFLOAT;
  minvoter = MAXFLOAT;

  do
  {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    if(mylocal.lid >= 0)
      //TODO: reduction..
      minvoter = std::min(minvoter, mylocal.myoutcost);
  } while(advanceThread());

  do
  {
    incost = minvoter;
    //check all dirs
    float newminval, nextnewminval;
    int2 newBlock;
    nextnewminval = newminval = 0.1f*MAXFLOAT;
    newBlock = int2(-1,-1);

    for(int outtest = 0; outtest < 8; ++outtest)
    {
      int2 testBlock;
      int testStart;
      testStart = -1;

      int outdir = 1 << outtest;
      do
      {
      LocalData& mylocal(locals[LocalData::getOffset()]);
      if(mylocal.myoutcost == minvoter && (mylocal.mydir & outdir))
      {
        int2 offset = getOffsetFromDir8Element(outtest);
        testBlock = currentBlock + offset;
        if(testBlock.x >= 0 && testBlock.y >= 0 &&
           testBlock.x < numBlocks.x && testBlock.y < numBlocks.y)
        {
          int2 inLid = mylocal.lid2 - int2(offset.x*(blockSize.x-1), offset.y*(blockSize.y-1));
          testStart = borderId(inLid, blockSize);
        }
      }
      } while(advanceThread());


      if(testStart != -1)
      {
        float2 nextMinval = checkRoutes(testBlock, testStart, routecost, routeMap, layer, numBlocks, blockSize, routeDataPerNode);
        do
        {
        LocalData& mylocal(locals[LocalData::getOffset()]);
        if(nextMinval.x < newminval &&
           nextMinval.y <= newminval + 0.000001f &&
           testStart == mylocal.lid)
        {
          newminval = nextMinval.y;
          nextnewminval = nextMinval.x;
          newBlock = testBlock;
          localInPos = mylocal.lid2;
        }
        } while(advanceThread());
      }
    }


    currentBlock = newBlock;
    minvoter = nextnewminval;

    do
    {
    LocalData& mylocal(locals[LocalData::getOffset()]);
    if(currentBlock.x != -1 && mylocal.lid >= 0)
    {
      int mydata_offset = getCostGlobalId(mylocal.lid, currentBlock, blockSize, numBlocks);
      mylocal.myoutcost = routecost[layer*routeDataPerNode +  mydata_offset];
      if(mylocal.lid == 0)
        mylocal.written = writeOutBlock(interblockResult, interblockSize, layer, currentBlock, blockSize, numBlocks, int2(currentBlock.x*(blockSize.x-1),currentBlock.y*(blockSize.y-1)) + localInPos, mylocal.written);
    }
    } while(advanceThread());


  } while( !(isContainedInBlock(tos[layer], currentBlock, blockSize)  || minvoter >= incost ) );
}
#endif
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

      _cl_routeMap_buffer =  cl::Buffer(_cl_context, CL_MEM_READ_WRITE, _blocks[0]*_blocks[1]*boundaryElements*(boundaryElements+1)/2*sizeof(cl_float));
      computeAll = true;
    }




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

    cl_int bufferDim[2] = {
      static_cast<cl_int>(_buffer_width),
      static_cast<cl_int>(_buffer_height)
    };

    //update route map
    //_cl_updateRouteMap_kernel


    _cl_updateRouteMap_kernel.setArg(0, memory_gl[0]);
    _cl_updateRouteMap_kernel.setArg(1, _cl_lastCostMap_buffer);
    _cl_updateRouteMap_kernel.setArg(2, _cl_routeMap_buffer);
    _cl_updateRouteMap_kernel.setArg(3, 2 * sizeof(cl_int), bufferDim);
    _cl_updateRouteMap_kernel.setArg(4, computeAll);
    _cl_updateRouteMap_kernel.setArg(5, 2*(_blockSize[0]+2)*(_blockSize[1]+2)*sizeof(float), NULL);
    _cl_updateRouteMap_kernel.setArg(6, sizeof(int), NULL);


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


    ////debug
    //size_t size;
    //_cl_routeMap_buffer.getInfo(CL_MEM_SIZE, &size);
    //std::vector<float> h_routeMap_buffer(size/sizeof(float));
    //_cl_command_queue.enqueueReadBuffer(_cl_routeMap_buffer, true, 0, size, &h_routeMap_buffer[0]);
    //
    ////check routemap
    //size_t bels = 2*(_blockSize[0] + _blockSize[1] - 2);
    //for(size_t j = 0; j < _blocks[1]; ++j)
    //for(size_t i = 0; i < _blocks[0]; ++i)
    //  validateRouteMap(&h_routeMap_buffer[(i + j*_blocks[0])*bels*(bels+1)/2], bels);
    ////end debug


    //    //dummy call
    //float* costmap = new float[_buffer_width*_buffer_height];
    //_cl_command_queue.enqueueReadBuffer(_cl_lastCostMap_buffer, true, 0, _buffer_width*_buffer_height * sizeof(float), &costmap[0]);
    //float* lastcostmap = new float[_buffer_width*_buffer_height];
    //float* routemap = new float[_blocks[0]*_blocks[1]*2*(_blockSize[0] + _blockSize[1])*2*(_blockSize[0] + _blockSize[1])/2];
    //float* lmem = new float[2*(_blockSize[0]+2)*(_blockSize[1]+2)];
    //initKernelData(int2(_blockSize[0],_blockSize[1]), int2(_blocks[0], _blocks[1]));
    //costmap_dim = int2(_buffer_width, _buffer_height);

    //updateRouteMapDummy(costmap, lastcostmap, routemap, int2(_buffer_width, _buffer_height), computeAll, lmem);
    //delete[] costmap;
    //delete[] lastcostmap;
    //delete[] routemap;
    //delete[] lmem;
    ////


    static int count = 0;
    std::stringstream fname;
    fname << "costmap" << count++;
    dumpBuffer<float>(_cl_command_queue, _cl_lastCostMap_buffer, _buffer_width, _buffer_height, fname.str());
    std::stringstream fname2;
    fname2 << "routemap" << count;
    int boundaryElements = 2*(_blockSize[0] + _blockSize[1]-2);
    dumpBuffer<float>(_cl_command_queue, _cl_routeMap_buffer, _blocks[0]*boundaryElements, _blocks[1]*boundaryElements/2, fname2.str());


    cl_ulong start, end;
    std::cout << "CLInfo:\n";
    updateRouteMap_Event.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    updateRouteMap_Event.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    std::cout << " - updateRouteMap: " << (end-start)/1000000.0  << "ms\n";
  }

  bool getTargetRect(const std::vector<float2> &vertices, int4& target, int downsample, int buffer_width, int buffer_height)
  {
    target = int4(buffer_width-1, buffer_height-1,0,0);
    for( auto p = vertices.begin();
          p != vertices.end();
          ++p )
    {
      target.x = std::min<int>(target.x, p->x/downsample);
      target.y = std::min<int>(target.y, p->y/downsample);
      target.z = std::max<int>(target.z, p->x/downsample);
      target.w = std::max<int>(target.w, p->y/downsample);
    }

    target.x = std::max(target.x, 0);
    target.y = std::max(target.y, 0);
    target.z = std::min(target.z, buffer_width-1);
    target.w = std::min(target.w, buffer_height-1);
    return (target.z - target.x > 0 && target.w - target.y > 0);
  }

  template<class Vec2>
  float2 idToPos(Vec2 id, int downsample)
  {
    return float2(id.x,id.y) * downsample + 0.5f*downsample;
  }
  int2 posToId(float2 pos, int downsample)
  {
    int2 out;
    out.x = pos.x / downsample;
    out.y = pos.y / downsample;
    return out;
  }
  template<class Iterator>
  void addTrailFor(std::vector<float2>& trail, Iterator begin, int downsample, const int _blockSize[2])
  {
    Iterator it = begin + 2;
    float2 blockoffset(*it & 0xFFFF, (*it >> 16) & 0xFFFF);
    blockoffset.x *= _blockSize[0]-1;
    blockoffset.y *= _blockSize[1]-1;
    ++it;
    uint count = *it++;
    Iterator end = it + count;
    while(it != end)
    {
      float2 inneroffset(*it & 0xFFFF, (*it >> 16) & 0xFFFF);
      float2 point = blockoffset + inneroffset;
      point = idToPos(point,downsample);
      trail.push_back(point);
      ++it;
    }
  }
  void GPURouting::createRoutes(LinksRouting::LinkDescription::HyperEdge& hedge)
  {

    //int2 test;
    //int2 seed(2,2);
    //int2 dim(8, 10);
    //for(test.y = 0; test.y < dim.y; ++test.y)
    //{
    //  for(test.x = 0; test.x < dim.x; ++test.x)
    //  {
    //    int res = getActiveBlock(seed*8, test*8, dim);
    //    int2 reverse = activeBlockToId(seed*8, res, 0, dim);
    //    std::cout << res << '(' << reverse.x << ',' << reverse.y << ")\t";
    //  }
    //  std::cout << '\n';
    //}

    LevelNodeMap levelNodeMap;
    LevelHyperEdgeMap levelHyperEdgeMap;

    std::map< LinkDescription::Node*, size_t > nodeMemMap;
    std::map< LinkDescription::HyperEdge*, size_t > hyperEdgeMemMap;
    std::vector< std::vector<  LinkDescription::Node* > > levelNodes;
    std::vector< std::vector<  LinkDescription::HyperEdge* > > levelHyperEdges;

    int downsample = _subscribe_desktop->_data->width / _subscribe_costmap->_data->width;
    int boundaryElements = 2*(_blockSize[0] + _blockSize[1]-2);
    cl_ulong start, end;

    //there can be loops due to hyperedges connecting the same nodes, so we have to avoid double entries
    //the same way, one node could be put on different levels, so we need to analyse the nodes level first
    checkLevels(levelNodeMap, levelHyperEdgeMap, hedge);

    //remove routing info
    for(auto edgeIt = levelHyperEdgeMap.begin(); edgeIt != levelHyperEdgeMap.end(); ++edgeIt)
      edgeIt->first->removeRoutingInformation();

    //compute mem requirement and map
    size_t slices = 0;
    for(auto it = levelNodeMap.begin(); it != levelNodeMap.end(); ++it)
    {
      if(nodeMemMap.find(it->first) != nodeMemMap.end())
        continue;
      nodeMemMap.insert(std::make_pair(it->first, slices++));
      if(levelNodes.size() < it->second + 1)
        levelNodes.resize(it->second + 1);
      levelNodes[it->second].push_back(it->first);
    }
    for(auto it = levelHyperEdgeMap.begin(); it != levelHyperEdgeMap.end(); ++it)
    {
      if(hyperEdgeMemMap.find(it->first) != hyperEdgeMemMap.end())
        continue;
      hyperEdgeMemMap.insert(std::make_pair(it->first, slices++));
      if(levelHyperEdges.size() < it->second + 1)
        levelHyperEdges.resize(it->second + 1);
      levelHyperEdges[it->second].push_back(it->first);
    }




    //alloc Memory
    cl_int bufferDim[2] = {
      static_cast<cl_int>(_buffer_width),
      static_cast<cl_int>(_buffer_height)
    };
    int rowelements = _blocks[0]*(_blockSize[0]-1)+1;
    int colelements = (_blocks[0]+1)*(_blockSize[1]-2);

    int requiredElements = (_blocks[1]+1)*rowelements + _blocks[1]*colelements;
    cl::Buffer d_routingData(_cl_context, CL_MEM_READ_WRITE, sizeof(cl_float)*requiredElements*slices);



    //debug
    //std::map<int,int> ids;
    //for(int2 bid(0,0); bid.y < _blocks[1]; ++bid.y)
    //  for(bid.x = 0; bid.x < _blocks[0]; ++bid.x)
    //    for(int lid = 0; lid < 2*(_blockSize[0]+_blockSize[1]-2); ++lid)
    //    {
    //      int res = getCostGlobalId(lid, bid, int2(_blockSize[0], _blockSize[1]), int2(_blocks[0], _blocks[1]));
    //      auto found = ids.find(res);
    //      if(found != ids.end())
    //      {
    //        if(lid == 0 || lid == _blockSize[0]-1 || lid == _blockSize[0]+_blockSize[1]-2 || lid == 2*_blockSize[0]+_blockSize[1]-3)
    //        {
    //          if(++found->second > 4)
    //            std::cout << "id already contained more than 4 times: [" << bid.x << "," << bid.y << "]->(" << lid << "): " << res << std::endl;
    //        }
    //        else
    //          if(++found->second > 2)
    //            std::cout << "id already contained more than 2 times:: [" << bid.x << "," << bid.y << "]->(" << lid << "): " << res << std::endl;
    //      }
    //      else
    //        ids.insert(std::make_pair(res,1));
    //      if(res >= requiredElements)
    //        std::cout << "id too large: " << res << "( > " << requiredElements << ")" <<  std::endl;
    //    }


    //init the border costs for all nodes

    _cl_prepareBorderCosts_kernel.setArg(0,d_routingData);

    cl::Event prepareBorderCostsEvent;
    _cl_command_queue.enqueueNDRangeKernel
    (
      _cl_prepareBorderCosts_kernel,
      cl::NullRange,
      cl::NDRange(requiredElements, slices),
      cl::NullRange,
      0,
      &prepareBorderCostsEvent
    );
    _cl_command_queue.finish();
    prepareBorderCostsEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
    prepareBorderCostsEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
    std::cout << " - prepareBorderCosts: " << (end-start)/1000000.0  << "ms\n";


    //bottom up routing -> for every level do:
    levelHyperEdges.resize(levelNodes.size()); //make same size (always: nodes.size > hyperedges.size)
    {
      auto levelNodesit = levelNodes.rbegin();
      auto levelHyperEdgesit = levelHyperEdges.rbegin();
      while(levelNodesit != levelNodes.rend() ||  levelHyperEdgesit != levelHyperEdges.rend())
      {
        std::vector<int4> routingPoints;
        std::vector<int4> startBlockRange;
        std::vector< std::vector<cl_uint> > routingSources;
        std::vector<cl_uint> routingIds;

        //geometry nodes
        for(auto nodeit = levelNodesit->begin(); nodeit != levelNodesit->end(); ++nodeit)
        {
          //handle non children first
          if((*nodeit)->getChildren().size() != 0)
            continue;

          int4 target;
          if(!getTargetRect((*nodeit)->getVertices(), target, downsample, _buffer_width, _buffer_height))
            continue;

          routingPoints.push_back(target);
          routingIds.push_back(nodeMemMap.find(*nodeit)->second);
          startBlockRange.push_back(int4(std::max(0,target.x/(_blockSize[0]-1)-1),
                                         std::max(0,target.y/(_blockSize[1]-1)-1),
                                         std::min(_blocks[0]-1,divup(target.z,_blockSize[0]-1)+1),
                                         std::min(_blocks[1]-1,divup(target.w,_blockSize[1]-1)+1) ) );
        }
        //nodes with hyperedges
        for(auto nodeit = levelNodesit->begin(); nodeit != levelNodesit->end(); ++nodeit)
        {
          if((*nodeit)->getChildren().size() == 0)
            continue;
          std::vector<cl_uint> thisSources;
          for(auto childIt = (*nodeit)->getChildren().begin();
              childIt != (*nodeit)->getChildren().end();
              ++childIt)
          {
            auto found = hyperEdgeMemMap.find(*childIt);
             if(found == hyperEdgeMemMap.end())
              continue;
            thisSources.push_back(found->second);
          }
          if(thisSources.size() == 0)
            continue;
          routingSources.push_back(thisSources);
          routingIds.push_back(nodeMemMap.find(*nodeit)->second);
          startBlockRange.push_back(int4(0,0, _blocks[0], _blocks[1]));
        }
        //nodes from hyperedges
        for(auto hyperedgeit = levelHyperEdgesit->begin(); hyperedgeit != levelHyperEdgesit->end(); ++hyperedgeit)
        {
          std::vector<cl_uint> thisSources;
          for(auto childIt = (*hyperedgeit)->getNodes().begin();
              childIt != (*hyperedgeit)->getNodes().end();
              ++childIt)
          {
            auto found = nodeMemMap.find(&(*childIt));
            if(found == nodeMemMap.end())
              continue;
            thisSources.push_back(found->second);
          }
          if(thisSources.size() == 0)
            continue;
          routingSources.push_back(thisSources);
          routingIds.push_back(hyperEdgeMemMap.find(*hyperedgeit)->second);
          startBlockRange.push_back(int4(0,0, _blocks[0], _blocks[1]));
        }

        cl::Buffer d_routingIds(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, routingIds.size()*sizeof(cl_uint), &routingIds[0]);

        if(routingPoints.size() > 0)
        {
          cl::Buffer d_routingPoints(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, routingPoints.size()*sizeof(uint4), &routingPoints[0]);
          // init all nodes with geometry
          std::vector<cl_int4> startingBlocks;
          //determine requ. blocks
          for(size_t i = 0; i < routingPoints.size(); ++i)
          {
            for(int y = routingPoints[i].y/(_blockSize[1]-1); y < divup(routingPoints[i].w,_blockSize[1]-1); ++y)
            for(int x = routingPoints[i].x/(_blockSize[0]-1); x < divup(routingPoints[i].z,_blockSize[0]-1); ++x)
              if(x >= 0 && x < _blocks[0] && y >= 0 && y < _blocks[1])
              {
                cl_int3 nblock;
                nblock.s[0] = x; nblock.s[1] = y; nblock.s[2] = i; nblock.s[3] = routingIds[i];
                startingBlocks.push_back(nblock);
              }
          }
          cl::Buffer d_prepareIndividualRoutingMapping(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, startingBlocks.size()*sizeof(cl_int4), &startingBlocks[0]);


          ////debug
          ////dummy call
          // float* costmap = new float[_buffer_width*_buffer_height];
          // _cl_command_queue.enqueueReadBuffer(_cl_lastCostMap_buffer, true, 0, _buffer_width*_buffer_height * sizeof(float), &costmap[0]);
          // float* routedata = new float[requiredElements*slices];
          // _cl_command_queue.enqueueReadBuffer(d_routingData, true, 0,_buffer_width*_buffer_height * sizeof(float), &routedata[0]);


          //  float* lmem = new float[2*(_blockSize[0]+2)*(_blockSize[1]+2)];
          //  initKernelData(int2(_blockSize[0],_blockSize[1]),int2(startingBlocks.size(),1) );

          //  prepareIndividualRoutingDummy(costmap, routedata, routingPoints, (int4*)&startingBlocks[0], int2(_buffer_width,_buffer_height), int2(_blocks[0], _blocks[1]), requiredElements, lmem);
          //  delete[] costmap;
          //  delete[] routedata;
          //  delete[] lmem;
          ////


          _cl_prepareIndividualRouting_kernel.setArg(0, _cl_lastCostMap_buffer);
          _cl_prepareIndividualRouting_kernel.setArg(1, d_routingData);
          _cl_prepareIndividualRouting_kernel.setArg(2, d_routingPoints);
          _cl_prepareIndividualRouting_kernel.setArg(3, d_prepareIndividualRoutingMapping);
          _cl_prepareIndividualRouting_kernel.setArg(4, 2 * sizeof(cl_int), bufferDim);
          _cl_prepareIndividualRouting_kernel.setArg(5, 2 * sizeof(cl_int), _blocks);
          _cl_prepareIndividualRouting_kernel.setArg(6, sizeof(cl_int), &requiredElements);
          _cl_prepareIndividualRouting_kernel.setArg(7, 2*(_blockSize[0]+2)*(_blockSize[1]+2)*sizeof(float), NULL);
          _cl_prepareIndividualRouting_kernel.setArg(8, sizeof(int), NULL);

          cl::Event prepareIndividualRoutingEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_prepareIndividualRouting_kernel,
            cl::NullRange,
            cl::NDRange(_blockSize[0]*startingBlocks.size(),_blockSize[1]),
            cl::NDRange(_blockSize[0],_blockSize[1]),
            0,
            &prepareIndividualRoutingEvent
          );

          _cl_command_queue.finish();
          prepareIndividualRoutingEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          prepareIndividualRoutingEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - prepareIndividualRouting: " << (end-start)/1000000.0  << "ms\n";

          ////debug:
          //std::vector<float> mem(requiredElements*slices);
          // _cl_command_queue.enqueueReadBuffer(d_routingData, true, 0, mem.size() * sizeof(float), &mem[0]);
          // for(size_t i = 0; i < mem.size(); ++i)
          //   if(mem[i] < 1000.0f)
          //    std::cout << "(" << i/requiredElements << "," << i % requiredElements << "): " << mem[i] << std::endl;
          //dumpBuffer<float>(_cl_command_queue, d_routingData, requiredElements, slices, "initRouting");
        }



        if(routingSources.size() > 0)
        {
          //init nodes from child routing data
          std::vector<cl_uint> routingSourcesOffset;
          std::vector<cl_uint> routingSourcesData;
          for(auto it = routingSources.begin(); it != routingSources.end(); ++it)
            routingSourcesOffset.push_back(routingSourcesData.size()),
            routingSourcesData.insert(routingSourcesData.end(), it->begin(), it->end());
          routingSourcesOffset.push_back(routingSourcesData.size());

          cl::Buffer d_routingSourcesData(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, routingSourcesData.size()*sizeof(cl_uint), &routingSourcesData[0]);
          cl::Buffer d_routingSourcesOffset(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, routingSourcesOffset.size()*sizeof(cl_uint), &routingSourcesOffset[0]);


          _cl_prepareIndividualRoutingParent_kernel.setArg(0, d_routingData);
          _cl_prepareIndividualRoutingParent_kernel.setArg(1, d_routingSourcesOffset);
          _cl_prepareIndividualRoutingParent_kernel.setArg(2, d_routingSourcesData);
          _cl_prepareIndividualRoutingParent_kernel.setArg(3, d_routingIds);
          _cl_prepareIndividualRoutingParent_kernel.setArg(4, (int)(routingIds.size() - routingSources.size()));
          _cl_prepareIndividualRoutingParent_kernel.setArg(5, requiredElements);
          _cl_prepareIndividualRoutingParent_kernel.setArg<float>(6, _Bvalue);

          cl::Event prepareIndividualRoutingParentEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_prepareIndividualRoutingParent_kernel,
            cl::NullRange,
            cl::NDRange(requiredElements,routingSources.size()),
            cl::NullRange,
            0,
            &prepareIndividualRoutingParentEvent
          );

          _cl_command_queue.finish();
          prepareIndividualRoutingParentEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          prepareIndividualRoutingParentEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - prepareIndividualRoutingParent: " << (end-start)/1000000.0  << "ms\n";
          //dumpBuffer<float>(_cl_command_queue, d_routingData, requiredElements, slices, "routingPrepareFromParent");
        }


        if(startBlockRange.size() > 0)
        {
          //run the routing

          //make sure we dont flood the queue
          int maxInitDim = sqrt(static_cast<float>(_routingQueueSize))/2;
          for(auto br_it = startBlockRange.begin(); br_it != startBlockRange.end(); ++br_it)
          {
            if(br_it->z-br_it->x > maxInitDim)
            {
              int center = (br_it->z+br_it->x)/2;
              br_it->x = center - maxInitDim/2;
              br_it->z = center + maxInitDim/2+1;
            }
            if(br_it->w-br_it->y > maxInitDim)
            {
              int center = (br_it->w+br_it->y)/2;
              br_it->y = center - maxInitDim/2;
              br_it->w = center + maxInitDim/2+1;
            }
          }
          cl::Buffer d_startBlockRange(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, startBlockRange.size()*sizeof(cl_int4), &startBlockRange[0]);


          ////debug
          ////dummy call
          //size_t size;
          //_cl_routeMap_buffer.getInfo(CL_MEM_SIZE, &size);
          //std::vector<float> h_routeMap_buffer(size/sizeof(float));
          //_cl_command_queue.enqueueReadBuffer(_cl_routeMap_buffer, true, 0, size, &h_routeMap_buffer[0]);
          //std::vector<float> h_routingData(requiredElements*slices);
          //_cl_command_queue.enqueueReadBuffer(d_routingData, true, 0, h_routingData.size() * sizeof(float), &h_routingData[0]);

          //int* data = new int[_routingNumLocalWorkers*(boundaryElements+1)];
          //uint* queue = new uint[_routingQueueSize];
          //int2 tactiveBufferSize(divup(_blocks[0],8), divup(_blocks[1],8));
          //std::vector<uint> activeBuffer(2*tactiveBufferSize.x*tactiveBufferSize.y*startBlockRange.size(), 0);
          //int localWorkerSize2 = divup(boundaryElements,_routingLocalWorkersWarpSize)*_routingLocalWorkersWarpSize;
          //initKernelData(int2(localWorkerSize2,1), int2(1,1));

          //////check routemap
          ////size_t bels = 2*(_blockSize[0] + _blockSize[1] - 2);
          ////for(size_t i = 0; i < _blocks[0]; ++i)
          ////  for(size_t j = 0; j < _blocks[1]; ++j)
          ////    validateRouteMap(&h_routeMap_buffer[(i + j*_blocks[0])*bels*(bels+1)/2], bels);

          //routeLocal(h_routeMap_buffer, startBlockRange, routingIds, h_routingData, requiredElements, int2(_blockSize[0], _blockSize[1]), int2(_blocks[0], _blocks[1]), data, queue, _routingQueueSize, &activeBuffer[0], tactiveBufferSize);

          //delete[] data;
          //delete[] queue;
          ////





          //active buffers
          cl_uint activeBufferSize[] = {divup(_blocks[0],8), divup(_blocks[1],8)};
          cl::Buffer d_routeActive(_cl_context, CL_MEM_READ_WRITE, 2*activeBufferSize[0]*activeBufferSize[1]*startBlockRange.size()*sizeof(cl_uint));
          _cl_initMem_kernel.setArg(0, d_routeActive);
          _cl_initMem_kernel.setArg(1, 0);

          cl::Event clearActiveBufferEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
           _cl_initMem_kernel,
            cl::NullRange,
            cl::NDRange(2*activeBufferSize[0],startBlockRange.size()*activeBufferSize[1]),
            cl::NullRange,
            0,
            &clearActiveBufferEvent
          );
          _cl_command_queue.finish();
          clearActiveBufferEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          clearActiveBufferEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - clearActiveBuffer: " << (end-start)/1000000.0  << "ms\n";



          _cl_routing_kernel.setArg(0, _cl_routeMap_buffer);
          _cl_routing_kernel.setArg(1, d_startBlockRange);
          _cl_routing_kernel.setArg(2, d_routingIds);
          _cl_routing_kernel.setArg(3, d_routingData);
          _cl_routing_kernel.setArg(4, requiredElements);
          _cl_routing_kernel.setArg(5, 2 * sizeof(cl_int), _blockSize);
          _cl_routing_kernel.setArg(6, 2 * sizeof(cl_int), _blocks);
          _cl_routing_kernel.setArg(7, _routingNumLocalWorkers*(boundaryElements+1)*sizeof(cl_float), NULL);
          _cl_routing_kernel.setArg(8, _routingQueueSize * sizeof(cl_int), NULL);
          _cl_routing_kernel.setArg(9, _routingQueueSize);
          _cl_routing_kernel.setArg(10, d_routeActive);
          _cl_routing_kernel.setArg(11, 2 * sizeof(cl_int), activeBufferSize);
          _cl_routing_kernel.setArg(12, boundaryElements * sizeof(cl_uint), NULL);

          int localWorkerSize = divup(boundaryElements,_routingLocalWorkersWarpSize)*_routingLocalWorkersWarpSize;

          cl::Event routingRoutingEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_routing_kernel,
            cl::NullRange,
            cl::NDRange(localWorkerSize*startBlockRange.size(),_routingNumLocalWorkers),
            cl::NDRange(localWorkerSize,_routingNumLocalWorkers),
            0,
            &routingRoutingEvent
          );
          _cl_command_queue.finish();
          routingRoutingEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          routingRoutingEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - routingRouting: " << (end-start)/1000000.0  << "ms\n";
          //printfBuffer<uint>(_cl_command_queue, d_routeActive, 2*activeBufferSize[0], activeBufferSize[1]*startBlockRange.size(), "activeBuffer");
          //dumpBuffer<float>(_cl_command_queue, d_routingData, requiredElements, slices, "routingRouting");
        }



        ++levelNodesit;
        ++levelHyperEdgesit;
      }
    }

    //top down route reconstruction
    {

      std::map<LinkDescription::HyperEdge*, int2> hyperEdgeCenters;
      std::map<LinkDescription::Node*, int2> nodePositions;
      auto levelNodesit = levelNodes.begin();
      auto levelHyperEdgesit = levelHyperEdges.begin();
      for( ; levelNodesit != levelNodes.end() ||  levelHyperEdgesit != levelHyperEdges.end() ;
          ++levelNodesit, ++levelHyperEdgesit )
      {
        std::vector<LinkDescription::Node* > needMinSearchNodes;
        std::vector<LinkDescription::HyperEdge* > needMinSearchHyperEdges;
        std::vector<uint> needMinSearchIds;
        std::vector<uint> needMinSearchOffsets;
        std::vector<uint> needMinSearchChildren;

        //for free nodes with no position find the minimum
        for(auto nodeit = levelNodesit->begin(); nodeit != levelNodesit->end(); ++nodeit)
          if((*nodeit)->getChildren().size() > 0 && nodePositions.find((*nodeit)) == nodePositions.end())
          {
            needMinSearchNodes.push_back(*nodeit);
            needMinSearchIds.push_back(nodeMemMap.find(*nodeit)->second);

            needMinSearchOffsets.push_back(needMinSearchChildren.size());
            for(auto childIt = (*nodeit)->getChildren().begin();
              childIt != (*nodeit)->getChildren().end();
              ++childIt)
            {
              auto found = hyperEdgeMemMap.find(*childIt);
              if(found != hyperEdgeMemMap.end())
                needMinSearchChildren.push_back(hyperEdgeMemMap.find(*childIt)->second);
            }
          }

        //for hyperedges with no parents find the minimum
        for(auto edgeit = levelHyperEdgesit->begin(); edgeit != levelHyperEdgesit->end(); ++edgeit)
          if((*edgeit)->getNodes().size() > 0 && hyperEdgeCenters.find(*edgeit) == hyperEdgeCenters.end())
          {
            needMinSearchHyperEdges.push_back(*edgeit);
            needMinSearchIds.push_back(hyperEdgeMemMap.find(*edgeit)->second);

            needMinSearchOffsets.push_back(needMinSearchChildren.size());
            for(auto childIt = (*edgeit)->getNodes().begin();
              childIt != (*edgeit)->getNodes().end();
              ++childIt)
            {
              auto found = nodeMemMap.find(&(*childIt));
              if(found != nodeMemMap.end())
                needMinSearchChildren.push_back(found->second);
            }
          }
        needMinSearchOffsets.push_back(needMinSearchChildren.size());

        if(needMinSearchIds.size() > 0)
        {
          cl::Buffer d_needMinSearchIds(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, needMinSearchIds.size()*sizeof(uint), &needMinSearchIds[0]);
          cl::Buffer d_needMinSearchOffsets(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, needMinSearchOffsets.size()*sizeof(uint), &needMinSearchOffsets[0]);
          cl::Buffer d_needMinSearchChildren(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, needMinSearchChildren.size()*sizeof(uint), &needMinSearchChildren[0]);

          std::vector<float> voteMin(needMinSearchIds.size(), 99999999.f);
          cl::Buffer d_voteMin(_cl_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, voteMin.size()*sizeof(float), &voteMin[0]);

          _cl_voteMinimum_kernel.setArg(0, d_routingData);
          _cl_voteMinimum_kernel.setArg(1, d_needMinSearchIds);
          _cl_voteMinimum_kernel.setArg(2, requiredElements);
          _cl_voteMinimum_kernel.setArg(3, 2 * sizeof(cl_int), _blockSize);
          _cl_voteMinimum_kernel.setArg(4, 2 * sizeof(cl_int), _blocks);
          _cl_voteMinimum_kernel.setArg(5, d_voteMin);
          _cl_voteMinimum_kernel.setArg(6, sizeof(float)*boundaryElements, NULL);

          cl::Event routingVoteMinEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_voteMinimum_kernel,
            cl::NullRange,
            cl::NDRange(boundaryElements*_blocks[0],_blocks[1],needMinSearchIds.size()),
            cl::NDRange(boundaryElements,1,1),
            0,
            &routingVoteMinEvent
          );
          _cl_command_queue.finish();
          //debug
          _cl_command_queue.enqueueReadBuffer(d_voteMin, true, 0, voteMin.size()*sizeof(float), &voteMin[0]);
          //
          routingVoteMinEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          routingVoteMinEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - routingVoteMin: " << (end-start)/1000000.0  << "ms\n";


          int maxResults = 3*32+1;
          std::vector<uint> minSearchResults(needMinSearchIds.size()*maxResults, 0xFFFFFFFF);
          for(size_t i = 0; i < needMinSearchIds.size(); ++i)
            minSearchResults[i*maxResults] = 1;
          cl::Buffer d_minSearchResults(_cl_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, minSearchResults.size()*sizeof(uint), &minSearchResults[0]);

          _cl_getMinimum_kernel.setArg(0, _cl_lastCostMap_buffer);
          _cl_getMinimum_kernel.setArg(1, d_routingData);
          _cl_getMinimum_kernel.setArg(2, d_needMinSearchIds);
          _cl_getMinimum_kernel.setArg(3, d_needMinSearchChildren);
          _cl_getMinimum_kernel.setArg(4, d_needMinSearchOffsets);
          _cl_getMinimum_kernel.setArg(5, requiredElements);
          _cl_getMinimum_kernel.setArg(6, 2 * sizeof(cl_int), _blockSize);
          _cl_getMinimum_kernel.setArg(7, 2 * sizeof(cl_int), _blocks);
          _cl_getMinimum_kernel.setArg(8, 2 * sizeof(cl_int), bufferDim);
          _cl_getMinimum_kernel.setArg(9, d_voteMin);
          _cl_getMinimum_kernel.setArg(10, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
          _cl_getMinimum_kernel.setArg(11, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
          _cl_getMinimum_kernel.setArg(12, d_minSearchResults);
          _cl_getMinimum_kernel.setArg(13, maxResults);
          _cl_getMinimum_kernel.setArg(14, sizeof(float)*_blockSize[0]*_blockSize[1], NULL);
          _cl_getMinimum_kernel.setArg(15, sizeof(int), NULL);

          cl::Event routingGetMinEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_getMinimum_kernel,
            cl::NullRange,
            cl::NDRange(_blockSize[0]*_blocks[0],_blockSize[1]*_blocks[1],needMinSearchIds.size()),
            cl::NDRange(_blockSize[0],_blockSize[1],1),
            0,
            &routingGetMinEvent
          );
          _cl_command_queue.finish();
          routingGetMinEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          routingGetMinEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - routingGetMin: " << (end-start)/1000000.0  << "ms\n";

          _cl_command_queue.enqueueReadBuffer(d_minSearchResults, true, 0, minSearchResults.size()*sizeof(uint), &minSearchResults[0]);
          auto thisdatastart = minSearchResults.begin();
          for(auto nit = needMinSearchNodes.begin(); nit != needMinSearchNodes.end(); ++nit)
          {
            //find min among all results
            auto thisdataend = thisdatastart + maxResults;
            float min_cost = 99999999.f;
            auto resContainer = nodePositions.insert(std::pair<LinkDescription::Node*, int2>(*nit, int2(0,0))).first;
            for(++thisdatastart; thisdatastart < thisdataend; thisdatastart+=3)
            {
              if( *thisdatastart == 0xFFFFFFFF)
                break;
              if(min_cost > *reinterpret_cast<float*>(&(*thisdatastart)))
              {
                min_cost = *reinterpret_cast<float*>(&(*thisdatastart));
                resContainer->second.x= *(thisdatastart+1);
                resContainer->second.y = *(thisdatastart+2);
              }
            }
            thisdatastart = thisdataend;
          }
          for(auto eit = needMinSearchHyperEdges.begin(); eit != needMinSearchHyperEdges.end(); ++eit)
          {
            //find min among all results
            auto thisdataend = thisdatastart + maxResults;
            float min_cost = 99999999.f;
            auto resContainer = hyperEdgeCenters.insert(std::pair<LinkDescription::HyperEdge*, int2>(*eit, int2(0,0))).first;
            for(++thisdatastart; thisdatastart < thisdataend; thisdatastart+=3)
            {
              if( *thisdatastart == 0xFFFFFFFF)
                break;
              if(min_cost > *reinterpret_cast<float*>(&(*thisdatastart)))
              {
                min_cost = *reinterpret_cast<float*>(&(*thisdatastart));
                resContainer->second.x = *(thisdatastart+1);
                resContainer->second.y = *(thisdatastart+2);
              }
            }
            thisdatastart = thisdataend;
          }
        }






        std::vector< LinkDescription::Node* > needRouteConstructionNodes;
        std::vector< LinkDescription::HyperEdge* > needRouteConstructionEdges;

        std::vector< uint4 > needRouteConstructionInfo;
        std::vector< uint >  needRouteConstructionElements;
        std::vector< int4 >  needRouteConstructionEndElements;

        //for free nodes go to child hyperedges
        for(auto nodeit = levelNodesit->begin(); nodeit != levelNodesit->end(); ++nodeit)
          if((*nodeit)->getChildren().size() > 0)
          {
            auto found = nodePositions.find((*nodeit));

            needRouteConstructionNodes.push_back(*nodeit);
            //TODO: use this when implementing bundling -> all routes from the same groups should be handled by one block
            //needRouteConstructionInfo.push_back(uint4(found->second[0], found->second[1], needRouteConstructionElements.size(),0));
            for(auto childIt = (*nodeit)->getChildren().begin();
              childIt != (*nodeit)->getChildren().end();
              ++childIt)
            {
              //TODO: remove this for implementing bundling
              needRouteConstructionInfo.push_back(uint4(found->second[0], found->second[1], 0,0));
              needRouteConstructionElements.push_back(hyperEdgeMemMap.find(*childIt)->second);
              needRouteConstructionEndElements.push_back(int4(-1,-1,-1,-1));
            }
          }

        //for hyperedges go to nodes
        for(auto edgeit = levelHyperEdgesit->begin(); edgeit != levelHyperEdgesit->end(); ++edgeit)
          if((*edgeit)->getNodes().size() > 0)
          {
            auto found = hyperEdgeCenters.find((*edgeit));

            needRouteConstructionEdges.push_back(*edgeit);
            //TODO: use this when implementing bundling -> all routes from the same groups should be handled by one block
            //needRouteConstructionInfo.push_back(uint4(found->second[0], found->second[1], needRouteConstructionElements.size(),0));
            for(auto childIt = (*edgeit)->getNodes().begin();
              childIt != (*edgeit)->getNodes().end();
              ++childIt)
            {
              //pushback startnodes
              int4 target;
              if(!getTargetRect(childIt->getVertices(), target, downsample, _buffer_width, _buffer_height))
                continue;
              //TODO: remove this for implementing bundling
              needRouteConstructionInfo.push_back(uint4(found->second[0], found->second[1], 0,0));
              needRouteConstructionEndElements.push_back(target);
              needRouteConstructionElements.push_back(nodeMemMap.find(&(*childIt))->second);
            }
          }

        needRouteConstructionInfo.push_back(uint4(0,0, needRouteConstructionElements.size(),0));

        if(needRouteConstructionElements.size() > 0)
        {

          ////debug
          ////dummy call
          //{
          //size_t size;
          //_cl_routeMap_buffer.getInfo(CL_MEM_SIZE, &size);
          //std::vector<float> h_routeMap_buffer(size/sizeof(float));
          //_cl_command_queue.enqueueReadBuffer(_cl_routeMap_buffer, true, 0, size, &h_routeMap_buffer[0]);
          //std::vector<float> h_routingData(requiredElements*slices);
          //_cl_command_queue.enqueueReadBuffer(d_routingData, true, 0, h_routingData.size() * sizeof(float), &h_routingData[0]);

          //_cl_lastCostMap_buffer.getInfo(CL_MEM_SIZE, &size);
          //std::vector<float> h_lastCostMap_buffer(size/sizeof(float));
          //_cl_command_queue.enqueueReadBuffer(_cl_lastCostMap_buffer, true, 0, size, &h_lastCostMap_buffer[0]);
          //
          //std::vector<uint> blockRoutes(needRouteConstructionElements.size()*_blocks[0]*_blocks[1]/4);
          //float* l_cost = new float[(_blockSize[0]+2)*(_blockSize[1]+2)],
          //     * l_route = new float[(_blockSize[0]+2)*(_blockSize[1]+2)];

          //initKernelData(int2(_blockSize[0],_blockSize[1]), int2(1,1));
          //calcInterBlockRouteDummy(&h_routingData[0],
          //                    &h_routeMap_buffer[0],
          //                    &h_lastCostMap_buffer[0],
          //                    &needRouteConstructionInfo[0],
          //                    &needRouteConstructionEndElements[0],
          //                    (int*)&needRouteConstructionElements[0],
          //                    requiredElements,
          //                    int2(_blockSize[0], _blockSize[1]),
          //                    int2(_blocks[0], _blocks[1]),
          //                    int2(bufferDim[0], bufferDim[1]),
          //                    &blockRoutes[0],
          //                    _blocks[0]*_blocks[1]/4,
          //                    l_cost,
          //                    l_route);
          //delete[] l_cost;
          //delete[] l_route;
          //}
          ////

          cl::Buffer d_needRouteConstructionInfo(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, needRouteConstructionInfo.size()*sizeof(uint4), &needRouteConstructionInfo[0]);
          cl::Buffer d_needRouteConstructionEndElements(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, needRouteConstructionEndElements.size()*sizeof(uint4), &needRouteConstructionEndElements[0]);
          cl::Buffer d_needRouteConstructionElements(_cl_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, needRouteConstructionElements.size()*sizeof(uint4), &needRouteConstructionElements[0]);

          int maxBlocksForRoute = _blocks[0]*_blocks[1]/4;
          cl::Buffer d_blockRoutes(_cl_context, CL_MEM_READ_WRITE, needRouteConstructionElements.size()*maxBlocksForRoute*sizeof(uint));

          _cl_routeInterBlock_kernel.setArg(0, d_routingData);
          _cl_routeInterBlock_kernel.setArg(1, _cl_routeMap_buffer);
          _cl_routeInterBlock_kernel.setArg(2, _cl_lastCostMap_buffer);
          _cl_routeInterBlock_kernel.setArg(3, d_needRouteConstructionInfo);
          _cl_routeInterBlock_kernel.setArg(4, d_needRouteConstructionEndElements);
          _cl_routeInterBlock_kernel.setArg(5, d_needRouteConstructionElements);
          _cl_routeInterBlock_kernel.setArg(6, requiredElements);
          _cl_routeInterBlock_kernel.setArg(7, 2 * sizeof(cl_int), _blockSize);
          _cl_routeInterBlock_kernel.setArg(8, 2 * sizeof(cl_int), _blocks);
          _cl_routeInterBlock_kernel.setArg(9, 2 * sizeof(cl_int), bufferDim);
          _cl_routeInterBlock_kernel.setArg(10, d_blockRoutes);
          _cl_routeInterBlock_kernel.setArg(11, maxBlocksForRoute);
          _cl_routeInterBlock_kernel.setArg(12, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
          _cl_routeInterBlock_kernel.setArg(13, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
          _cl_routeInterBlock_kernel.setArg(14, sizeof(float)*_blockSize[0]*_blockSize[1], NULL);
          _cl_routeInterBlock_kernel.setArg(15, sizeof(int)*2, NULL);
          _cl_routeInterBlock_kernel.setArg(16, sizeof(float)*4, NULL);
          _cl_routeInterBlock_kernel.setArg(17, sizeof(cl_int2)*3, NULL);

          cl::Event routingRouteInterBlockEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_routeInterBlock_kernel,
            cl::NullRange,
            cl::NDRange(_blockSize[0],_blockSize[1],needRouteConstructionElements.size()),
            cl::NDRange(_blockSize[0],_blockSize[1],1),
            0,
            &routingRouteInterBlockEvent
          );
          _cl_command_queue.finish();
          routingRouteInterBlockEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          routingRouteInterBlockEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - routingRouteInterBlock: " << (end-start)/1000000.0  << "ms\n";

          std::vector<uint> blockRoutes(needRouteConstructionElements.size()*maxBlocksForRoute);
          _cl_command_queue.enqueueReadBuffer(d_blockRoutes, true, 0, blockRoutes.size()*sizeof(uint), &blockRoutes[0]);

          ////debug
          //uint j = 0;
          //for(auto it = blockRoutes.begin(); it != blockRoutes.end(); it += maxBlocksForRoute, ++j)
          //{
          //  uint rblocks = *it;
          //  std::cout << j << ": route blocks: " << *it << "\n";
          //  for(int i = 0; i < rblocks; ++i)
          //  {
          //    uint block = *(it + 2*i+1);
          //    uint interblock = *(it + 2*i + 2);
          //    int2 gid((block & 0xFFFF) , (block >> 16)& 0xFFFF);
          //    std::cout << "(" << gid.x << "," << gid.y << ")";
          //    gid.x = gid.x*(_blockSize[0]-1) + (interblock & 0xFFFF);
          //    gid.y = gid.y*(_blockSize[1]-1) + ((interblock >> 16)& 0xFFFF);
          //    std::cout << "[" << gid.x << "," << gid.y << "] -> ";
          //  }
          //  std::cout << "[" << needRouteConstructionEndElements[j].x << " - " << needRouteConstructionEndElements[j].z << ", "
          //    << needRouteConstructionEndElements[j].y << " - " << needRouteConstructionEndElements[j].w << "]\n";
          //}
          ////

          //compute requirements for launch
          uint maxBlocks = 0;
          uint sumBlocks = 0;
          for(auto it = blockRoutes.begin(); it != blockRoutes.end(); it += maxBlocksForRoute)
          {
            sumBlocks += *it;
            maxBlocks = std::max(*it, maxBlocks);
          }

          uint innerBlockRoutesSize = sumBlocks*(4 + 3*(_blockSize[0] + _blockSize[1])/2);
          cl::Buffer d_innerBlockRoutes(_cl_context, CL_MEM_READ_WRITE, innerBlockRoutesSize*sizeof(uint));
          uint one = 1;
          _cl_command_queue.enqueueWriteBuffer(d_innerBlockRoutes, true, 0, sizeof(uint), &one);

          _cl_routeConstruct_kernel.setArg(0, d_routingData);
          _cl_routeConstruct_kernel.setArg(1, _cl_lastCostMap_buffer);
          _cl_routeConstruct_kernel.setArg(2, d_needRouteConstructionEndElements);
          _cl_routeConstruct_kernel.setArg(3, d_needRouteConstructionElements);
          _cl_routeConstruct_kernel.setArg(4, requiredElements);
          _cl_routeConstruct_kernel.setArg(5, 2 * sizeof(cl_int), _blockSize);
          _cl_routeConstruct_kernel.setArg(6, 2 * sizeof(cl_int), _blocks);
          _cl_routeConstruct_kernel.setArg(7, 2 * sizeof(cl_int), bufferDim);
          _cl_routeConstruct_kernel.setArg(8, d_blockRoutes);
          _cl_routeConstruct_kernel.setArg(9, maxBlocksForRoute);
          _cl_routeConstruct_kernel.setArg(10, d_innerBlockRoutes);
          _cl_routeConstruct_kernel.setArg(11, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
          _cl_routeConstruct_kernel.setArg(12, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
          _cl_routeConstruct_kernel.setArg(13, sizeof(int), NULL);

          cl::Event routingRouteConstructEvent;
          _cl_command_queue.enqueueNDRangeKernel
          (
            _cl_routeConstruct_kernel,
            cl::NullRange,
            cl::NDRange(_blockSize[0]*maxBlocks,_blockSize[1],needRouteConstructionElements.size()),
            cl::NDRange(_blockSize[0],_blockSize[1],1),
            0,
            &routingRouteConstructEvent
          );
          _cl_command_queue.finish();
          routingRouteConstructEvent.getProfilingInfo(CL_PROFILING_COMMAND_END, &end);
          routingRouteConstructEvent.getProfilingInfo(CL_PROFILING_COMMAND_START, &start);
          std::cout << " - routingRouteConstruct: " << (end-start)/1000000.0  << "ms\n";

          std::vector<uint> innerBlockRoutes(innerBlockRoutesSize);
          _cl_command_queue.enqueueReadBuffer(d_innerBlockRoutes, true, 0, innerBlockRoutesSize*sizeof(uint), &innerBlockRoutes[0]);


          std::vector< std::map<uint, uint> > innerBlockRoutesOffsets(needRouteConstructionElements.size());
          auto ibrIt = innerBlockRoutes.begin();
          auto ibrEnd = ibrIt + *ibrIt;
          ibrIt++;
          uint offset = 1;
          while(ibrIt != ibrEnd)
          {
            uint edge = *ibrIt++;
            uint segmentInEdge = *ibrIt++;
            ++ibrIt;
            uint count = *ibrIt++;
            ibrIt += count;
            //std::cout << edge << "->" << segmentInEdge << "\n";
            innerBlockRoutesOffsets[edge].insert(std::pair<uint,uint>(segmentInEdge, offset));
            offset += count + 4;
          }
          ////debug
          //ibrIt = innerBlockRoutes.begin()+1;
          //while(ibrIt != ibrEnd)
          //{
          //
          //  std::cout << "found route segment:\n";
          //  std::cout << "(edge: " << *ibrIt++;
          //  std::cout << ", offset: " << *ibrIt++;
          //  int2 blockId = int2(*ibrIt & 0xFFFF, (*ibrIt >> 16) & 0xFFFF);
          //  std::cout << ", block: " << blockId.x << "," << blockId.y << ")   ";
          //  ++ibrIt;
          //  uint count = *ibrIt++;
          //  for(uint i = 0; i < count; ++i, ++ibrIt)
          //  {
          //    int2 innerOffset(*ibrIt & 0xFFFF, (*ibrIt >> 16) & 0xFFFF);
          //    int2 point;
          //    point.x = blockId.x * (_blockSize[0]-1) + innerOffset.x;
          //    point.y = blockId.y * (_blockSize[1]-1) + innerOffset.y;
          //    std::cout << "[" << point.x << "," << point.y << "]->";
          //  }
          //  std::cout << "\n";
          //}
          ////

          //put the routes together
          uint edge = 0;
          for( auto needRouteNodesIt = needRouteConstructionNodes.begin();
            needRouteNodesIt != needRouteConstructionNodes.end();
            ++needRouteNodesIt)
          {
            auto &nodeChildren((*needRouteNodesIt)->getChildren());
            for(auto childrenIt = nodeChildren.begin();
              childrenIt != nodeChildren.end();
              ++childrenIt)
            {
              if((*childrenIt)->getHyperEdgeDescription() == 0)
                (*childrenIt)->setHyperEdgeDescription
                (
                  std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>()
                );
              LinkDescription::HyperEdgeDescriptionForkationPtr fork = (*childrenIt)->getHyperEdgeDescription();
              fork->incoming.trail.clear();

              float2 startpos = idToPos(nodePositions.find(*needRouteNodesIt)->second, downsample);
              fork->incoming.trail.push_back(startpos);
              auto iBRBegin = innerBlockRoutes.begin();
              for(auto edgesegmentit = innerBlockRoutesOffsets[edge].begin();
                edgesegmentit != innerBlockRoutesOffsets[edge].end();
                ++edgesegmentit)
                addTrailFor(fork->incoming.trail, iBRBegin + edgesegmentit->second, downsample, _blockSize);
              ++edge;
              fork->position = fork->incoming.trail.back();
              hyperEdgeCenters.insert(std::make_pair(*childrenIt, posToId(fork->incoming.trail.back(), downsample)));
            }
          }
          for( auto needRouteEdgesIt = needRouteConstructionEdges.begin();
            needRouteEdgesIt != needRouteConstructionEdges.end();
            ++needRouteEdgesIt)
          {
            auto &hyperedgeChildren((*needRouteEdgesIt)->getNodes());
            if((*needRouteEdgesIt)->getHyperEdgeDescription() == 0)
                (*needRouteEdgesIt)->setHyperEdgeDescription
                (
                  std::make_shared<LinkDescription::HyperEdgeDescriptionForkation>()
                );
            LinkDescription::HyperEdgeDescriptionForkationPtr fork = (*needRouteEdgesIt)->getHyperEdgeDescription();
            fork->position = idToPos(hyperEdgeCenters.find((*needRouteEdgesIt))->second, downsample);

            for(auto childrenIt = hyperedgeChildren.begin();
              childrenIt != hyperedgeChildren.end();
              ++childrenIt)
            {
              fork->outgoing.push_back(LinkDescription::HyperEdgeDescriptionSegment());
              LinkDescription::HyperEdgeDescriptionSegment& segment(fork->outgoing.back());
              segment.nodes.push_back(*childrenIt);
              segment.trail.push_back(fork->position);

              auto iBRBegin = innerBlockRoutes.begin();
              for(auto edgesegmentit = innerBlockRoutesOffsets[edge].begin();
                edgesegmentit != innerBlockRoutesOffsets[edge].end();
                ++edgesegmentit)
                addTrailFor(segment.trail, iBRBegin + edgesegmentit->second, downsample, _blockSize);
              ++edge;
              nodePositions.insert(std::make_pair(&(*childrenIt), posToId(segment.trail.back(), downsample)));
            }
          }
        }
      }
    }

  }

}
