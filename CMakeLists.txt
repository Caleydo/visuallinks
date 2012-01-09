project(LinksSystem)
cmake_minimum_required(VERSION 2.6)

set(PROJROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LINKS_INCLUDE_DIR ${PROJROOT}/include)
set(COMPONENTS_DIR ${PROJROOT}/components)
set(COMPONENTS_INC_DIR )
set(COMPONENTS_LIBS )
set(BIN_DIR ${PROJROOT}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJROOT}/cmake)

#set interface files
set( INTERFACE_FILES
  ${LINKS_INCLUDE_DIR}/core.h
  ${LINKS_INCLUDE_DIR}/component.h
  ${LINKS_INCLUDE_DIR}/config.h
  ${LINKS_INCLUDE_DIR}/costanalysis.h
  ${LINKS_INCLUDE_DIR}/renderer.h
  ${LINKS_INCLUDE_DIR}/routing.h
  ${LINKS_INCLUDE_DIR}/transparencyanalysis.h
  ${LINKS_INCLUDE_DIR}/proxy.h
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
include_directories(${COMPONENTS_DIR}/tools)

#TODO we should do that in a better way, so we can get the include directories and libs to link out of the subdirectory..
#add components
add_subdirectory(${COMPONENTS_DIR}/staticcore ${BIN_DIR}/staticcore)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/staticcore/include)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} staticcore)

add_subdirectory(${COMPONENTS_DIR}/xmlconfig ${BIN_DIR}/xmlconfig)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/xmlconfig/include)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} xmlconfig)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/xmlconfig/)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} tinyxml)

add_subdirectory(${COMPONENTS_DIR}/costanalysis ${BIN_DIR}/costanalysis)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/costanalysis/include)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} glcostanalysis)

add_subdirectory(${COMPONENTS_DIR}/renderer_gl ${BIN_DIR}/renderer_gl)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/renderer_gl/include)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} glrenderer)

add_subdirectory(${COMPONENTS_DIR}/routing_cpu ${BIN_DIR}/routing_cpu)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/routing_cpu/include)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} cpurouting)

add_subdirectory(${COMPONENTS_DIR}/routing_gpu ${BIN_DIR}/routing_gpu)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/routing_gpu/include)
set(COMPONENTS_LIBS ${COMPONENTS_LIBS} gpurouting)

add_subdirectory(${COMPONENTS_DIR}/routing_ice_proxy ${BIN_DIR}/routing_ice_proxy)
#set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/routing_ice_proxy/include)
#set(COMPONENTS_LIBS ${COMPONENTS_LIBS} gpurouting)

add_subdirectory(${COMPONENTS_DIR}/transparencyanalysis ${BIN_DIR}/transparencyanalysis)
set(COMPONENTS_INC_DIR ${COMPONENTS_INC_DIR} ${COMPONENTS_DIR}/transparencyanalysis/include)
#set(COMPONENTS_LIBS ${COMPONENTS_LIBS} gpurouting)



#add default applications
add_subdirectory(${PROJROOT}/qtfullscreensystem ${BIN_DIR}/qtfullscreensystem)
add_subdirectory(${PROJROOT}/qt-ocr ${BIN_DIR}/qt-ocr)
#add_subdirectory(${PROJROOT}/singleimagesystem ${BIN_DIR}/singleimagesystem)


#add interface files to the project
add_custom_target(InterfaceFiles SOURCES ${INTERFACE_FILES})
#add interface files to the project
add_custom_target(CommonFiles SOURCES ${COMMON_FILES})