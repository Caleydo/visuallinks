

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
include_directories(${LINKS_INCLUDE_DIR} ${COMPONENTINC_DIR})

set( HEADER_FILES
  include/staticcore.h
)


set( SOURCE_FILES
  src/slots.cpp
  src/staticcore.cpp
)

add_library(staticcore ${HEADER_FILES} ${SOURCE_FILES})

add_component_data(${COMPONENTINC_DIR} staticcore)
