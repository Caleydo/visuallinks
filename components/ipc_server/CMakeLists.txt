set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)

include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTINC_DIR}
  ${PROJROOT}/components/tools
)

set(HEADER_FILES
  include/ClientInfo.hxx
  include/ipc_server.hpp
  include/window_monitor.hpp
)

set(SOURCE_FILES
  src/ClientInfo.cxx
  src/ipc_server.cpp
  src/window_monitor.cpp
)

set(CL_FILES
#  ${COMPONENTROOT}/routing.cl
#  ${COMPONENTROOT}/sorting.cl
)

add_definitions(-DQXT_STATIC)

add_library(ipc_server ${HEADER_FILES} ${SOURCE_FILES} ${CL_FILES})
qt5_use_modules(ipc_server Script Widgets OpenGL Network WebSockets)
target_link_libraries( ipc_server
  qxt
  tools-qt
)
add_component_data(${COMPONENTINC_DIR} ipc_server)