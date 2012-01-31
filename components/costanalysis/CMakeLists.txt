

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(${LINKS_INCLUDE_DIR} ${COMPONENTINC_DIR})

if(WIN32)
find_package(GLEW REQUIRED)
include_directories( ${GLEW_INCLUDE_DIR} )
set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${GLEW_LIBRARIES})
endif(WIN32)

set(HEADER_FILES ${COMPONENTINC_DIR}/glcostanalysis.h
    )

set(SOURCE_FILES ${COMPONENTSRC_DIR}/glcostanalysis.cpp
    )

set(SHADER_FILES ${COMPONENTROOT}/featureMap.glsl
				 ${COMPONENTROOT}/saliencyFilter.glsl)

add_library(glcostanalysis ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})
target_link_libraries(glcostanalysis
  tools
  glsl
  ${ADDITIONAL_LIBS}
)
add_component_data(${COMPONENTINC_DIR} glcostanalysis SHADER_FILES)