project(glsl)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})
set(LocalGLSL_libs glsl)
set(LocalGLSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIR})
set(LocalGLSL_libs ${LocalGLSL_libs} ${GLEW_LIBRARIES})
set(LocalGLSL_INCLUDE_DIR ${LocalGLSL_INCLUDE_DIR} ${GLEW_INCLUDE_DIR})
endif(WIN32)

add_library(glsl glinfo.h glsl.h glutils.h glinfo.cpp glsl.cpp glutils.cpp)

set(GLSL_LIBRARIES ${LocalGLSL_libs} CACHE STRING "Libs for glsl" FORCE)
set(GLSL_INCLUDE_DIR ${LocalGLSL_INCLUDE_DIR} CACHE STRING "GLSL include dir" FORCE)
MARK_AS_ADVANCED(GLSL_LIBRARIES GLSL_INCLUDE_DIR)