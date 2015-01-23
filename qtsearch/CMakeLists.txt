# files
set( HEADER_FILES
#  include/screenshot_widget.hpp
)

set( HEADER_FILES_MOC
  include/text_widget.hpp
)

set( SOURCE_FILES
  src/main.cpp
  src/text_widget.cpp
)

set( FIREFOX_ADDON_CONTENT ${PROJROOT}/addons/firefox/src/content)
set( RUNTIME_FILES
  ${FIREFOX_ADDON_CONTENT}/icon-16.png
  ${FIREFOX_ADDON_CONTENT}/icon-active-16.png
  ${FIREFOX_ADDON_CONTENT}/icon-error-16.png
)

# dependencies & build
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

include_directories(
  ${LINKS_INCLUDE_DIR}
  ${QT_INCLUDES}
  include
)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})
set( HEADER_FILES 
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)

add_executable(qtsearch ${HEADER_FILES} ${SOURCE_FILES})
qt5_use_modules(qtsearch Gui Network WebSockets)

use_components(qtsearch)
runtime_files(qtsearch RUNTIME_FILES) # TODO just copy needed files
