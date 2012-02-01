

set(COMPONENTROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENTSRC_DIR ${COMPONENTROOT}/src)
set(COMPONENTINC_DIR ${COMPONENTROOT}/include)
set(REQ_INC_DIRS ${COMPONENTINC_DIR} )
set(REQ_LIBS   xmlconfig)

include_directories(${LINKS_INCLUDE_DIR} ${COMPONENTINC_DIR})
ADD_DEFINITIONS( -DTIXML_USE_STL )


find_package(TinyXML)
if(TINY_XML_FOUND)
message("using system's tinyxml library")
set(REQ_INC_DIRS ${REQ_INC_DIRS} ${TINY_XML_INCLUDE_DIR} )
set(REQ_LIBS ${REQ_LIBS} ${TINY_XML_LIBRARY})
include_directories(${TINY_XML_INCLUDE_DIR})
else()
message("using accompanying tinyxml library")
add_subdirectory(tinyxml)
include_directories(${COMPONENTROOT})
set(REQ_INC_DIRS ${REQ_INC_DIRS} ${COMPONENTROOT} )
set(REQ_LIBS ${REQ_LIBS} tinyxml )
endif()


set(HEADER_FILES ${COMPONENTINC_DIR}/xmlconfig.h
    )

set(SOURCE_FILES ${COMPONENTSRC_DIR}/xmlconfig.cpp
    )

add_library(xmlconfig ${HEADER_FILES} ${SOURCE_FILES})

set(REQ_INC_DIRS ${COMPONENTINC_DIR} ${COMPONENTROOT})
set(REQ_LIBS  tinyxml xmlconfig)
add_component_data(REQ_INC_DIRS REQ_LIBS)