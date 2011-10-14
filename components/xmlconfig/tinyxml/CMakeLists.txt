PROJECT(tinyxml)

SET(tinyxml_SOURCES
  tinystr.h
  tinyxml.h
  tinyxml.cpp
  tinyxmlerror.cpp
  tinyxmlparser.cpp
  tinystr.cpp
)
ADD_DEFINITIONS( -DTIXML_USE_STL )

add_library(tinyxml STATIC ${tinyxml_SOURCES})
