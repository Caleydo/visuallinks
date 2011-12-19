

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTINC_DIR}
  ${PROJROOT}/components/tools
)

set(HEADER_FILES
  ${COMPONENTINC_DIR}/glrenderer.h
)


set(SOURCE_FILES
  ${COMPONENTSRC_DIR}/glrenderer.cpp
)

add_library(glrenderer ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(glrenderer tools)
