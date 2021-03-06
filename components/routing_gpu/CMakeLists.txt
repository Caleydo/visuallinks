find_package(OpenCL REQUIRED)

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTINC_DIR}
  ${OPENCL_INCLUDE_DIRS}
  ${PROJROOT}/components/tools
)

set(HEADER_FILES
  ${COMPONENTINC_DIR}/gpurouting.h
)

set(SOURCE_FILES
  ${COMPONENTSRC_DIR}/gpurouting.cpp
)

set(CL_FILES
  ${COMPONENTROOT}/routing.cl
  ${COMPONENTROOT}/sorting.cl
)

add_library(gpurouting ${HEADER_FILES} ${SOURCE_FILES} ${CL_FILES})
target_link_libraries(gpurouting ${OPENCL_LIBRARIES})
add_component_data(${COMPONENTINC_DIR} gpurouting CL_FILES)
