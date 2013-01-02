set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)

find_package(Qt4 REQUIRED)
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTINC_DIR}
  ${PROJROOT}/components/tools
  ${QT_INCLUDES}
)

set(HEADER_FILES
#  ${COMPONENTINC_DIR}/ipc_server.hpp
)

set( HEADER_FILES_MOC
  include/ipc_server.hpp
  include/window_monitor.hpp
)

set(SOURCE_FILES
  src/ipc_server.cpp
  src/window_monitor.cpp
)

set(CL_FILES
#  ${COMPONENTROOT}/routing.cl
#  ${COMPONENTROOT}/sorting.cl
)

add_definitions(-DQXT_STATIC)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})
set( HEADER_FILES 
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)

add_library(ipc_server ${HEADER_FILES} ${SOURCE_FILES} ${CL_FILES})
target_link_libraries( ipc_server
  QtWebSocket
  qxt
)
add_component_data(${COMPONENTINC_DIR} ipc_server)