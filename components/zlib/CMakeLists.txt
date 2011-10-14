project(zlib)

file(GLOB zlib_SOURCES *.c)

include_directories(.)
add_library(zlib STATIC ${zlib_SOURCES})
