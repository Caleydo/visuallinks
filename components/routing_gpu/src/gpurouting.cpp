#include "gpurouting.h"

#include <iostream>
#include <fstream>
#include <cmath>

#if defined(__APPLE__) || defined(__MACOSX)
# error "Context sharing not supported yet."
#elif defined(_WIN32)
//# include <GL/wgl.h>
#else
# include <GL/glx.h>
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
  }

  bool GPURouting::startup(Core* core, unsigned int type)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  void GPURouting::init()
  {

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


    _cl_command_queue = cl::CommandQueue(_cl_context, _cl_device);

    // -----------------------------
    // And now the OpenCL program


    std::ifstream source_file("routing.cl");
    if( !source_file )
      throw std::runtime_error("Failed to open routing.cl");
    std::string source( std::istreambuf_iterator<char>(source_file),
                       (std::istreambuf_iterator<char>()) );

    cl::Program::Sources sources;
    sources.push_back( std::make_pair(source.c_str(), source.length()) );

    _cl_program = cl::Program(_cl_context, sources);

    try
    {
      _cl_program.build(devices);
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

    _cl_prepare_kernel = cl::Kernel(_cl_program, "prepareRouting");
    _cl_shortestpath_kernel = cl::Kernel(_cl_program, "routing");
  }
  void GPURouting::shutdown()
  {

  }

  void GPURouting::process(Type type)
  {
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

    //the costs
    unsigned int numBlocks[2] = {divup<unsigned>(width,_blockSize[0]),divup<unsigned>(height,_blockSize[1])};
    unsigned int sumBlocks = numBlocks[0]*numBlocks[1];
    cl::Buffer buf(_cl_context, CL_MEM_READ_WRITE, num_points * h_targets.size() * sizeof(float));

    cl::Buffer queue_pos(_cl_context, CL_MEM_READ_WRITE, sumBlocks * h_targets.size() * sizeof(unsigned int));
    cl::Buffer queue(_cl_context, CL_MEM_READ_WRITE, sumBlocks * h_targets.size() * sizeof(cl_uint4));
    cl::Buffer targets(_cl_context, CL_MEM_READ_WRITE, h_targets.size()*sizeof(cl_uint4));
    _cl_command_queue.enqueueWriteBuffer(targets, true, 0, h_targets.size()*sizeof(cl_uint4), &h_targets[0]);

    _cl_prepare_kernel.setArg(0, memory_gl[0]);
    _cl_prepare_kernel.setArg(1, buf);
    _cl_prepare_kernel.setArg(2, queue);
    _cl_prepare_kernel.setArg(3, queue_pos);
    _cl_prepare_kernel.setArg(4, targets);
    int numtargets = h_targets.size();
    _cl_prepare_kernel.setArg(5, sizeof(int), &numtargets);
    int dim[] = {width, height};
    _cl_prepare_kernel.setArg(6, 2 * sizeof(int), dim);
    _cl_prepare_kernel.setArg(7, 2 * sizeof(int), numBlocks);

    _cl_command_queue.enqueueNDRangeKernel
    (
      _cl_prepare_kernel,
      cl::NullRange,
      cl::NDRange(_blockSize[0]*numBlocks[0],_blockSize[1]*numBlocks[1],numtargets),
      cl::NDRange(_blockSize[0], _blockSize[1], 1)
    );
    

    _cl_shortestpath_kernel.setArg(0, memory_gl[0]);
    _cl_shortestpath_kernel.setArg(1, buf);
    _cl_shortestpath_kernel.setArg(2, queue);
    _cl_shortestpath_kernel.setArg(3, queue_pos);
    _cl_shortestpath_kernel.setArg(4, targets);
    _cl_shortestpath_kernel.setArg(5, sizeof(int), &numtargets);
    _cl_shortestpath_kernel.setArg(6, 2 * sizeof(int), dim);
    _cl_shortestpath_kernel.setArg(7, 2 * sizeof(int), numBlocks);
    _cl_shortestpath_kernel.setArg(8, sizeof(float)*(_blockSize[0]+2)*(_blockSize[1]+2), NULL);
    
    for(int iteration = 0; iteration < 200; ++iteration)
    {
      _cl_command_queue.enqueueNDRangeKernel
      (
        _cl_shortestpath_kernel,
        cl::NullRange,
        cl::NDRange(_blockSize[0]*numBlocks[0],_blockSize[1]*numBlocks[1],numtargets),
        cl::NDRange(_blockSize[0], _blockSize[1], 1)
      );
    }


    _cl_command_queue.enqueueReleaseGLObjects(&memory_gl);
    _cl_command_queue.finish();

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
