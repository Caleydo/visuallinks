# files
set( HEADER_FILES
  include/qglwidget.hpp
)

set( HEADER_FILES_MOC
  include/application.hpp
)

set( SOURCE_FILES
  src/application.cpp
  src/main.cpp
  src/qglwidget.cpp
)

set( SHADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/simple.vert
	${CMAKE_CURRENT_SOURCE_DIR}/remove_links.frag
)

set (ADDITIONAL_INCLUDE_DIRS
)
set (ADDITIONAL_LIBS
)

# dependencies & build
find_package(OpenGL REQUIRED)
find_package(Qt4 REQUIRED)
find_package(OpenCL REQUIRED)

# windows stuff
if(WIN32)
find_package(GLEW REQUIRED)
set(ADDITIONAL_INCLUDE_DIRS ${ADDITIONAL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIR})
set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${GLEW_LIBRARIES})
endif(WIN32)

include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})


set(SYSTEMROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTS_INC_DIR}
  ${SYSTEMROOT}
  ${QT_INCLUDES}
  ${OPENGL_INCLUDE_DIR}
  ${OPENCL_INCLUDE_DIRS}
  ${ADDITIONAL_INCLUDE_DIRS}
  include
)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})
set( HEADER_FILES 
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)

add_executable(qtfullscreensystem ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})
target_link_libraries( qtfullscreensystem
  ${OPENGL_gl_LIBRARY}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
  ${ADDITIONAL_LIBS}
)
use_components(qtfullscreensystem)
runtime_files(qtfullscreensystem SHADER_FILES)