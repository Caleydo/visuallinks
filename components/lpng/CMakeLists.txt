project(lpng)

file(GLOB lpng_SOURCES *.c)

include_directories(../zlib)
add_library(lpng STATIC ${lpng_SOURCES})
