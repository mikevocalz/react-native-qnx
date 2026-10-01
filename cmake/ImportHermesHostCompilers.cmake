# Imported host compiler used while CMake cross-compiles Hermes for QNX.
#
# Hermes includes this file through IMPORT_HOST_COMPILERS. Only hermesc is
# imported because the QNX patch disables the native InternalJavaScript
# shermes path unless HERMESVM_INTERNAL_JAVASCRIPT_NATIVE is explicitly enabled.

if(NOT DEFINED RNQNX_HOST_HERMESC OR RNQNX_HOST_HERMESC STREQUAL "")
  message(FATAL_ERROR "RNQNX_HOST_HERMESC must point at the host hermesc executable")
endif()

if(NOT EXISTS "${RNQNX_HOST_HERMESC}")
  message(FATAL_ERROR "Host hermesc does not exist: ${RNQNX_HOST_HERMESC}")
endif()

if(NOT TARGET imported-hermesc)
  add_executable(imported-hermesc IMPORTED GLOBAL)
  set_target_properties(imported-hermesc PROPERTIES
    IMPORTED_LOCATION "${RNQNX_HOST_HERMESC}")
endif()
