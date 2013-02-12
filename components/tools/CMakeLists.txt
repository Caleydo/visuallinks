
set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(
  ${LINKS_INCLUDE_DIR}
)
message("inc=${LINKS_INCLUDE_DIR}")

if(WIN32)
find_package(GLEW REQUIRED)
include_directories(
  ${GLEW_INCLUDE_DIR}
)
endif(WIN32)

set(HEADER_FILES
  fbo.h
)

set(SOURCE_FILES
  fbo.cxx
  HierarchicTileMap.cxx
  linkdescription.cpp
  PartitionHelper.cxx
  Rect.cxx
)

set(HEADER_FILES_QT
)

set(SOURCE_FILES_QT
  qt_helper.cxx
)

add_subdirectory(glsl)

add_definitions(-DNOMULTISAMPLING)
add_library(tools ${HEADER_FILES} ${SOURCE_FILES})

find_package(Qt4 REQUIRED)
include_directories(${QT_INCLUDES})
add_library(tools-qt ${HEADER_FILES_QT} ${SOURCE_FILES_QT})


