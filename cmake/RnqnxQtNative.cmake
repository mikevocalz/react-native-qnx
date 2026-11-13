include_guard(GLOBAL)

# v2 native primitives are independently opt-in. Turning everything off keeps
# the existing QNX/Marathon Qt graph and package behavior unchanged.
option(RNQNX_ENABLE_QT_MOTION "Build experimental native Qt motion primitives" OFF)
option(RNQNX_ENABLE_QT_SVG "Build the experimental native SVG document component" OFF)
option(RNQNX_ENABLE_QT_GRAPHICS "Build the experimental Qt graphics surface observer" OFF)
# Needs the Qt Multimedia add-on build (docs/getting-started/media.md).
option(RNQNX_ENABLE_QT_VIDEO "Build the Qt Multimedia video component (QtVideo)" OFF)

if((RNQNX_ENABLE_QT_MOTION OR RNQNX_ENABLE_QT_SVG OR RNQNX_ENABLE_QT_GRAPHICS OR RNQNX_ENABLE_QT_VIDEO)
   AND NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin|QNX)$")
  message(FATAL_ERROR "Qt native v2 experiments require a Linux, macOS or QNX Qt host")
endif()

foreach(feature MOTION SVG GRAPHICS VIDEO)
  if(RNQNX_ENABLE_QT_${feature})
    string(TOLOWER "${feature}" leaf)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native/qt/${leaf}"
                     "${CMAKE_BINARY_DIR}/qt-native/${leaf}")
    if(COMMAND rnqnx_enable_strict_warnings)
      rnqnx_enable_strict_warnings(rnqnx_qt_${leaf})
    endif()
    if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
      add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../tests/native/qt-${leaf}"
                       "${CMAKE_BINARY_DIR}/qt-native/${leaf}-tests")
    endif()
  endif()
endforeach()
