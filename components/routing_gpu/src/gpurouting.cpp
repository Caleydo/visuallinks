#include "gpurouting.h"
#include "log.hpp"

#include <iostream>
#include <fstream>
#include <cmath>


#if defined(__APPLE__) || defined(__MACOSX)
# error "Context sharing not supported yet."
#elif defined(_WIN32)
//# include <GL/wgl.h>
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
    _subscribe_search_id =
      slot_subscriber.getSlot<std::string>("/links[0]/id");
    _subscribe_search_stamp =
      slot_subscriber.getSlot<uint32_t>("/links[0]/stamp");
    _subscribe_search_regions =
      slot_subscriber.getSlot<std::vector<SlotType::Polygon>>("/links[0]/regions");
  }

  bool GPURouting::startup(Core* core, unsigned int type)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  void GPURouting::init()
  {
    _last_search_id.clear();
    _last_search_stamp = 0;
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

    //test
    int blocksize = 64;
    int datasize = 4*blocksize;
    cl::Kernel sorting_test = cl::Kernel(_cl_program, "sortTest");
    cl::Buffer keys(_cl_context, CL_MEM_READ_WRITE, datasize*sizeof(cl_float));
    cl::Buffer values(_cl_context, CL_MEM_READ_WRITE, datasize*sizeof(cl_uint));
    std::vector<float> h_keys;
    std::vector<int> h_values;
    h_keys.reserve(datasize);
    h_values.reserve(datasize);
    for(int i = 0; i < datasize; ++i)
    {
      int v = rand();
      h_keys.push_back(v/1000.0f);
      h_values.push_back(v);
    }

    _cl_command_queue.enqueueWriteBuffer(keys, true, 0, datasize*sizeof(cl_float), &h_keys[0]);
    _cl_command_queue.enqueueWriteBuffer(values, true, 0, datasize*sizeof(cl_uint), &h_values[0]);

    sorting_test.setArg(0, keys);
    sorting_test.setArg(1, values);
    sorting_test.setArg(2, 2*blocksize*sizeof(cl_float), NULL);
    sorting_test.setArg(3, 2*blocksize*sizeof(cl_uint), NULL);

   _cl_command_queue.enqueueNDRangeKernel
    (
      sorting_test,
      cl::NullRange,
      cl::NDRange(datasize/2),
      cl::NDRange(blocksize)
    );

    _cl_command_queue.finish();
    _cl_command_queue.enqueueReadBuffer(keys, true, 0, datasize*sizeof(cl_float), &h_keys[0]);
    _cl_command_queue.enqueueReadBuffer(values, true, 0, datasize*sizeof(cl_uint), &h_values[0]);

    for(int i = 0; i < datasize; ++i)
      std::cout <<  "<" << h_keys[i] << "," << h_values[i] << ">,\n";
    std::cout << std::endl;
  }
  void GPURouting::shutdown()
  {

  }

  //----------------------------------------------------------------------------
  void GPURouting::process(Type type)
  {
    if(    !_subscribe_search_id->isValid()
        || !_subscribe_search_stamp->isValid()
        || !_subscribe_search_regions->isValid() )
    {
      LOG_INFO("No valid routing data available.");
      return;
    }

    if(    _last_search_id == *_subscribe_search_id->_data
        && _last_search_stamp == *_subscribe_search_stamp->_data )
    {
      LOG_INFO("Nothing new to route.");
      return;
    }

    _last_search_id = *_subscribe_search_id->_data;
    _last_search_stamp = *_subscribe_search_stamp->_data;

    LOG_INFO("New routing data: " << _last_search_id);

    for(SlotType::Polygon poly: *_subscribe_search_regions->_data)
    {
      std::cout << "Polygon: (" << poly.points.size() << " points)\n";
      for(auto p: poly.points)
        std::cout << " (" << p.x << "|" << p.y << ")\n";
      std::cout << std::endl;
    }

    if( !_enabled ) // This can be set from the config file...
      return;

    try
    {

    if( !_subscribe_costmap->isValid() )
    {
      std::cerr << "GPURouting: No valid costmap received." << std::endl;
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

    unsigned int width = _subscribe_costmap->_data->width,
                 height = _subscribe_costmap->_data->height,
                 num_points = width * height;

    //testdata:
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
    std::vector<uint4> h_targets;
    h_targets.push_back(uint4(10*width/100, 11*height/100, 10*width/100+10, 11*height/100+10));
    h_targets.push_back(uint4(90*width/100, 90*height/100, 90*width/100+10, 90*height/100+10));
    h_targets.push_back(uint4(91*width/100, 10*height/100, 91*width/100+10, 10*height/100+10));
    //


    //structs used in kernel
    typedef cl_int4 cl_QueueElement;
    struct cl_QueueGlobal
    {
      cl_uint front;
      cl_uint back;
      cl_int filllevel;
      cl_uint activeBlocks;
      cl_uint processedBlocks;
      cl_uint sortingBarrier;
      cl_int debug;
    };

    //std::cout << sizeof(cl_QueueElement) << " " << sizeof(cl_QueueGlobal) << std::endl;

    //setup queue
    int numtargets = h_targets.size();
    int dim[] = {width, height};
    unsigned int numBlocks[2] = {divup<unsigned>(width,_blockSize[0]),divup<unsigned>(height,_blockSize[1])};
    unsigned int sumBlocks = numBlocks[0]*numBlocks[1];
    unsigned int overAllBlocks = sumBlocks * numtargets;
    unsigned int blockProcessThreshold = overAllBlocks*40;
    cl::Buffer buf(_cl_context, CL_MEM_READ_WRITE, num_points * numtargets * sizeof(float));

    cl::Buffer queue_info(_cl_context, CL_MEM_READ_WRITE, sizeof(cl_QueueGlobal));
    cl::Buffer queue_priority(_cl_context, CL_MEM_READ_WRITE, overAllBlocks * sizeof(cl_float));
    cl::Buffer queue(_cl_context, CL_MEM_READ_WRITE, overAllBlocks *sizeof(cl_QueueElement));
    cl::Buffer targets(_cl_context, CL_MEM_READ_WRITE,numtargets*sizeof(cl_uint4));

    _cl_command_queue.enqueueWriteBuffer(targets, true, 0, numtargets*sizeof(cl_uint4), &h_targets[0]);

    cl_QueueGlobal baseQueueInfo;
    baseQueueInfo.front = 0;
    baseQueueInfo.back = 0;
    baseQueueInfo.filllevel = 0;
    baseQueueInfo.activeBlocks = 0;
    baseQueueInfo.processedBlocks = 0;
    baseQueueInfo.sortingBarrier = overAllBlocks + 1;
    baseQueueInfo.debug = 0;

    _cl_command_queue.enqueueWriteBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);

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

    _cl_command_queue.enqueueReadBuffer(queue_info, true, 0, sizeof(cl_QueueGlobal), &baseQueueInfo);
    std::cout << "front: " << baseQueueInfo.front << " "
              << "back: " << baseQueueInfo.back << " "
              << "filllevel: " << baseQueueInfo.filllevel << " "
              << "activeBlocks: " << baseQueueInfo.activeBlocks << " "
              << "processedBlocks: " << baseQueueInfo.processedBlocks << " "
              << "debug: " << baseQueueInfo.debug << "\n";
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

    static int c = 8;
    if( !--c )
    {
      std::vector<float> host_mem(num_points*numtargets);
      _cl_command_queue.enqueueReadBuffer(buf, true, 0, num_points * numtargets * sizeof(float), &host_mem[0]);

      std::ofstream fimg("test.pfm", std::ios_base::out | std::ios_base::binary | std::ios_base::trunc );
      fimg << "PF\n";
      fimg << width << " " << height*numtargets << '\n';
      fimg << -1.0f << '\n';

      //std::ofstream img("test.pgm", std::ios_base::trunc);
      //img << "P2\n"
      //    << width << " " << height*numtargets << "\n"
      //    << "255\n";

      for( unsigned int i = 0; i < num_points*numtargets; ++i )
      {
        //img << host_mem[i] << "\n";
        for(int j = 0; j < 3; ++j)
          fimg.write(reinterpret_cast<char*>(&host_mem[i]),sizeof(float));
      }
      throw std::runtime_error("stop");
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
