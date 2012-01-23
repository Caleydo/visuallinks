

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(${LINKS_INCLUDE_DIR} ${COMPONENTINC_DIR})

set(HEADER_FILES ${COMPONENTINC_DIR}/glcostanalysis.h
    )
	
if(WIN32)
find_package(GLEW REQUIRED)
include_directories( ${GLEW_INCLUDE_DIR} )
set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${GLEW_LIBRARIES})
endif(WIN32)

set(SOURCE_FILES ${COMPONENTSRC_DIR}/glcostanalysis.cpp
    )

add_library(glcostanalysis ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(glcostanalysis
  tools
  glsl
  ${ADDITIONAL_LIBS}
)
