

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTINC_DIR}
  ${PROJROOT}/components/tools
)

if(WIN32)
find_package(GLEW REQUIRED)
include_directories( ${GLEW_INCLUDE_DIR} )
set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${GLEW_LIBRARIES})
endif(WIN32)

set(HEADER_FILES
  ${COMPONENTINC_DIR}/glrenderer.h
)


set(SOURCE_FILES
  ${COMPONENTSRC_DIR}/glrenderer.cpp
)

set(SHADER_FILES ${COMPONENTROOT}/blurX.glsl
                 ${COMPONENTROOT}/blurY.glsl)

add_library(glrenderer ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(glrenderer tools ${ADDITIONAL_LIBS})

add_component_data(${COMPONENTINC_DIR} glrenderer SHADER_FILES)
