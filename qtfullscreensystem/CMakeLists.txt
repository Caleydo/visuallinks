find_package(OpenGL REQUIRED)
find_package(Qt4 REQUIRED)

set(SYSTEMROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTS_INC_DIR}
  ${SYSTEMROOT}
  ${QT_INCLUDES}
  ${OPENGL_INCLUDE_DIR}
)

set( HEADER_FILES
  qglwidget.h
)

set( SOURCE_FILES
  main.cpp
  qglwidget.cpp
)

add_executable(qtfullscreensystem ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries( qtfullscreensystem
  ${COMPONENTS_LIBS}
  ${OPENGL_gl_LIBRARY}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
)