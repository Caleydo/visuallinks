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
  src/FastMatchTemplate.cpp
)

# dependencies & build
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

# OpenCV support
find_package(OpenCV REQUIRED)
include_directories(${OPENCV_INCLUDE_DIR})

include_directories(
  ${QT_INCLUDES}
  include
)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})
set( HEADER_FILES 
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)

add_executable(qt-ncc ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries( qt-ncc
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  opencv_core
  opencv_highgui
  opencv_imgproc
)
