
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
  Rect.cxx
)

add_definitions(-DNOMULTISAMPLING)
add_library(tools ${HEADER_FILES} ${SOURCE_FILES})
add_subdirectory(glsl)

