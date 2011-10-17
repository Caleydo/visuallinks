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

# dependencies & build
find_package(OpenGL REQUIRED)
find_package(Qt4 REQUIRED)

include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

set(SYSTEMROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTS_INC_DIR}
  ${SYSTEMROOT}
  ${QT_INCLUDES}
  ${OPENGL_INCLUDE_DIR}
  include
)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})
set( HEADER_FILES 
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)

add_executable(qtfullscreensystem ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries( qtfullscreensystem
  ${COMPONENTS_LIBS}
  ${OPENGL_gl_LIBRARY}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
)