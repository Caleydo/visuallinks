# files
set( HEADER_FILES
  include/application.hpp
#  include/qglwidget.hpp
  include/Shader.hpp
  include/Window.hpp
)

set( SOURCE_FILES
  src/application.cpp
  src/main.cpp
#  src/qglwidget.cpp
#  src/render_thread.cpp
  src/Shader.cpp
  src/Window.cpp
)

set( SHADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/simple.vert
	${CMAKE_CURRENT_SOURCE_DIR}/remove_links.frag
	${CMAKE_CURRENT_SOURCE_DIR}/blend.frag
)

set( RUNTIME_FILES
  ${SHADER_FILES}
  ${CMAKE_CURRENT_SOURCE_DIR}/qtconfig.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/wikipedia-test.png
  ${CMAKE_CURRENT_SOURCE_DIR}/C-130J-wikipedia.png
)

set (ADDITIONAL_INCLUDE_DIRS
)
set (ADDITIONAL_LIBS
)

# dependencies & build
find_package(OpenGL REQUIRED)

if(LinksComponent_routing_gpu)
  find_package(OpenCL REQUIRED)
  include_directories(${OPENCL_INCLUDE_DIRS})
endif()

# windows stuff
if(WIN32)
  find_package(GLEW REQUIRED)
  set(ADDITIONAL_INCLUDE_DIRS ${ADDITIONAL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIR})
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${GLEW_LIBRARIES})
endif(WIN32)

add_definitions(-DQXT_STATIC)

set(SYSTEMROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTS_INC_DIR}
  ${SYSTEMROOT}
  ${OPENGL_INCLUDE_DIR}
  ${ADDITIONAL_INCLUDE_DIRS}
  include
)

add_executable(qtfullscreensystem ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})
use_components(qtfullscreensystem)
qt5_use_modules(qtfullscreensystem Gui OpenGL Network)
target_link_libraries( qtfullscreensystem
  ${OPENGL_gl_LIBRARY}
  ${ADDITIONAL_LIBS}
)

runtime_files(qtfullscreensystem RUNTIME_FILES)
