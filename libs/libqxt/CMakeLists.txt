# files
set( HEADER_FILES
  include/QxtGui/QxtWindowSystem
  include/QxtGui/qxtwindowsystem.h
  include/qxtglobal.h
)

set( HEADER_FILES_MOC

)

set( SOURCE_FILES
  src/core/qxtglobal.cpp
  src/widgets/qxtwindowsystem.cpp
)

if(WIN32)
  set(SOURCE_FILES ${SOURCE_FILES} src/widgets/win/qxtwindowsystem_win.cpp)
elseif(APPLE)
  set(SOURCE_FILES ${SOURCE_FILES} src/widgets/x11/qxtwindowsystem_mac.cpp)
  set(HEADER_FILES ${HEADER_FILES} src/widgets/x11/qxtwindowsystem_mac.h)
else()
  set(SOURCE_FILES ${SOURCE_FILES} src/widgets/x11/qxtwindowsystem_x11.cpp)
endif()

# ...
find_package(Qt4 REQUIRED)
include_directories(
  ${QT_INCLUDE_DIR}
  include
  include/QxtGui
  src/core
  src/widgets
)
include(${QT_USE_FILE})

add_definitions(-DQT_NO_DEBUG)

QT4_WRAP_CPP(HEADER_FILES_MOC_GENERATED ${HEADER_FILES_MOC})

ADD_LIBRARY(qxt STATIC
  ${SOURCE_FILES}
  ${HEADER_FILES}
  ${HEADER_FILES_MOC_GENERATED}
)