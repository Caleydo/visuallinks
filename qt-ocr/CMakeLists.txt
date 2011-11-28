# files
set( HEADER_FILES
  include/screenshot_widget.hpp
)

set( HEADER_FILES_MOC
#  include/screenshot_widget.hpp
)

set( SOURCE_FILES
  src/main.cpp
  src/screenshot_widget.cpp
)

# dependencies & build
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

set(QXT_FIND_COMPONENTS QxtGui)
find_package(Qxt REQUIRED)

find_package(Tesseract REQUIRED)

set(SYSTEMROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(
  ${LINKS_INCLUDE_DIR}
  ${COMPONENTS_INC_DIR}
  ${SYSTEMROOT}
  ${QT_INCLUDES}
  ${QXT_INCLUDE_DIRS}
  ${TESSERACT_INCLUDE_DIR}
  include
)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})
set( HEADER_FILES 
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)

add_executable(qt-ocr ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries( qt-ocr
  ${COMPONENTS_LIBS}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QXT_LIBRARIES}
  ${TESSERACT_LIBRARY}
)