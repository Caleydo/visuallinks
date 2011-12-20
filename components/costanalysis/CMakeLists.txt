

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(${LINKS_INCLUDE_DIR} ${COMPONENTINC_DIR})

set(HEADER_FILES ${COMPONENTINC_DIR}/glcostanalysis.h
    )


set(SOURCE_FILES ${COMPONENTSRC_DIR}/glcostanalysis.cpp
    )

add_library(glcostanalysis ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(glcostanalysis
  tools
  glsl
)
