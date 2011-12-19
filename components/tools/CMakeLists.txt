include_directories(
  ${LINKS_INCLUDE_DIR}
)

set(HEADER_FILES
  fbo.h
)

set(SOURCE_FILES
  fbo.cxx
)

add_definitions(-DNOMULTISAMPLING)
add_library(tools ${HEADER_FILES} ${SOURCE_FILES})
add_subdirectory(glsl)
