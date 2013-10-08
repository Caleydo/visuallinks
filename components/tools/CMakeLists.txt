
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
  AnimatedPopup.cxx
  fbo.cxx
  HierarchicTileMap.cxx
  linkdescription.cpp
  PartitionHelper.cxx
  Rect.cxx
  routing.cxx
)

set(HEADER_FILES_QT
)

set(SOURCE_FILES_QT
  JSONParser.cxx
  qt_helper.cxx
)

add_subdirectory(glsl)

add_definitions(-DNOMULTISAMPLING)
add_library(tools ${HEADER_FILES} ${SOURCE_FILES})

add_library(tools-qt ${HEADER_FILES_QT} ${SOURCE_FILES_QT})
qt5_use_modules(tools-qt Script)