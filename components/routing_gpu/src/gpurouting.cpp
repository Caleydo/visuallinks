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

    _cl_kernel = cl::Kernel(_cl_program, "prepareRouting");
  }
  void GPURouting::shutdown()
  {

  }

  void GPURouting::process(Type type)
  {
    try
    {
    const GLint miplevel = 2; // 1/4 size

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
         miplevel,
         _subscribe_costmap->_data->id
       )
    );

    glFinish();
    _cl_command_queue.enqueueAcquireGLObjects(&memory_gl);

    unsigned int width = _subscribe_costmap->_data->width / (1 << miplevel),
                 height = _subscribe_costmap->_data->height / (1 << miplevel),
                 num_points = width * height;

    cl::Buffer buf(_cl_context, CL_MEM_READ_WRITE, num_points * sizeof(unsigned int));
    cl::Buffer queue(_cl_context, CL_MEM_READ_WRITE, num_points * sizeof(unsigned int));

    _cl_kernel.setArg(0, memory_gl[0]);
    _cl_kernel.setArg(1, buf);
    //_cl_kernel.setArg(2, queue);

    int dim[] = {width, height},
        start[] = {10, 20},
        target[] = {300, 200};

    _cl_kernel.setArg(2, 2 * sizeof(int), dim);
    //_cl_kernel.setArg(4, 2 * sizeof(int), start);
    //_cl_kernel.setArg(5, 2 * sizeof(int), target);
    //_cl_command_queue.finish();

    _cl_command_queue.enqueueNDRangeKernel
    (
      _cl_kernel,
      cl::NullRange,
      cl::NDRange(width,height),
      cl::NullRange
    );

    //_cl_command_queue.finish();
    _cl_command_queue.enqueueReleaseGLObjects(&memory_gl);
    _cl_command_queue.finish();

    static int c = 5;
    if( !--c )
    {
      std::vector<unsigned int> host_mem(num_points);
      _cl_command_queue.enqueueReadBuffer(buf, true, 0, num_points * sizeof(unsigned int), &host_mem[0]);

      std::ofstream img("test.pgm");
      img << "P2\n"
          << width << " " << height << "\n"
          << "65535\n";

      for( unsigned int i = 0; i < num_points; ++i )
        img << host_mem[i] << "\n";
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
