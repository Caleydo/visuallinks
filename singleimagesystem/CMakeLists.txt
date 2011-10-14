set(SYSTEMROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LINKS_INCLUDE_DIR} ${COMPONENTS_INC_DIR} ${SYSTEMROOT})

set(HEADER_FILES 
    )


set(SOURCE_FILES ${SYSTEMROOT}/main.cpp
    )


add_executable(singleimagesystem ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(singleimagesystem ${COMPONENTS_LIBS})