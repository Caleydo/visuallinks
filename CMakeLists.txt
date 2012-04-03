project(LinksSystem)
cmake_minimum_required(VERSION 2.6)

set(PROJROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LINKS_INCLUDE_DIR ${PROJROOT}/include)
set(COMPONENTS_DIR ${PROJROOT}/components)
set(COMPONENTS_INC_DIR )
set(COMPONENTS_LIBS )
set(BIN_DIR ${PROJROOT}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJROOT}/cmake)
set(LIBRARY_OUTPUT_PATH ${BIN_DIR}/lib) 

#include macros
include( cmake/CopyFiles.cmake )
include( cmake/AddComponent.cmake )
include( cmake/AddSubdirOption.cmake )

#set interface files
set( INTERFACE_FILES
  ${LINKS_INCLUDE_DIR}/core.h
  ${LINKS_INCLUDE_DIR}/component.h
  ${LINKS_INCLUDE_DIR}/config.h
  ${LINKS_INCLUDE_DIR}/costanalysis.h
  ${LINKS_INCLUDE_DIR}/renderer.h
  ${LINKS_INCLUDE_DIR}/routing.h
  ${LINKS_INCLUDE_DIR}/transparencyanalysis.h
  ${LINKS_INCLUDE_DIR}/color.h
  ${LINKS_INCLUDE_DIR}/linkdescription.h
)

#set common files
set(COMMON_FILES  ${LINKS_INCLUDE_DIR}/common/plainlinkdescription.h
                  ${LINKS_INCLUDE_DIR}/common/hashedmap.h
                  ${LINKS_INCLUDE_DIR}/common/componentarguments.h
                  )

if(WIN32)
  add_definitions( "/wd4250" )
endif(WIN32)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0 -Wall -O2 -g -std=c++0x")
endif()

add_subdirectory(${COMPONENTS_DIR}/tools)
include_directories(
  ${COMPONENTS_DIR}/tools
  libs/qtwebsocket/QtWebSocket
)

add_subdirectory(libs/qtwebsocket/QtWebSocket)

#TODO we should do that in a better way, so we can get the include directories and libs to link out of the subdirectory..
#add components
add_component(staticcore)
add_component(xmlconfig)
add_component(ipc_server)
add_component(costanalysis)
add_component(renderer_gl)
add_component(routing_cpu off)
add_component(routing_gpu)
add_component(transparencyanalysis off)
#message("comp inc:\n${COMPONENTS_INC_DIR} \ncomp libs:\n${COMPONENTS_LIBS}\ndepeping files:\n${COMPONENTS_DEPENDING_FILES}")

#add default applications
add_subdir_option(qtfullscreensystem QTFullscreenSystem)
add_subdir_option(qt-ncc QtNormalizedCrossCorrelationTest FALSE EXCLUDE_FROM_ALL)
add_subdir_option(qt-ocr QtOCRTest FALSE EXCLUDE_FROM_ALL)
#add_subdirectory(${PROJROOT}/singleimagesystem ${BIN_DIR}/singleimagesystem)


#add interface files to the project
add_custom_target(InterfaceFiles SOURCES ${INTERFACE_FILES})
#add interface files to the project
add_custom_target(CommonFiles SOURCES ${COMMON_FILES})
