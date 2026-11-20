include_guard(GLOBAL)

# Experimental MarathonOS native Linux Qt Wayland target.
# Links the existing Qt/React/Fabric host. No QNX SDP, libscreen or second
# component renderer is introduced.
#
# With RNQNX_MARATHON_HOST_NITRO=ON the host also links the Marathon Nitro v1
# adapter (native/platform/linux-host/nitro) and the shared Qt Nitro dispatcher
# (cmake/RnqnxNitroQt.cmake), installs both before the bundle runs, and accepts
# --nitro-proof. The QNX adapter targets are not part of this link graph.
function(rnqnx_add_marathon_host)
  if(TARGET rnqnx_marathon_host)
    return()
  endif()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "rnqnx_add_marathon_host only supports Linux")
  endif()
  if(NOT TARGET rnqnx_react_host)
    message(FATAL_ERROR "rnqnx_react_host must be built first")
  endif()
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/metro.marathon.config.js")
    message(FATAL_ERROR
      "Missing apps/rn-playground/metro.marathon.config.js, the Marathon Metro "
      "config the native Marathon bundle is built with")
  endif()
  if(RNQNX_MARATHON_HOST_NITRO AND NOT TARGET rnqnx_nitro_qt)
    message(FATAL_ERROR
      "RNQNX_MARATHON_HOST_NITRO needs the Qt Nitro dispatcher: configure with "
      "-DRNQNX_ENABLE_NITRO_QT_DISPATCHER=ON and an isolated copy of nitro-core "
      "in RNQNX_NITRO_CORE_ROOT (see cmake/RnqnxNitroQt.cmake)")
  endif()

  set(RNQNX_MARATHON_BUNDLE
    "${CMAKE_BINARY_DIR}/rnqnx-marathon-playground.bundle.js")

  add_custom_command(
    OUTPUT "${RNQNX_MARATHON_BUNDLE}"
    COMMAND
      "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.bin/metro"
      build index.js
      --config metro.marathon.config.js
      --platform marathon --dev false
      --out "${RNQNX_MARATHON_BUNDLE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/index.js"
      # Metro embeds dimensions and hashes from this PNG in the RN fixture.
      # Track it exactly as the original QNX bundle target does, so changing
      # the asset always invalidates the Marathon bundle in incremental builds.
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/qnx-asset.png"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/rn-playground/metro.marathon.config.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/metro-config-marathon/src/index.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/react-native-marathon/Libraries/Utilities/Platform.js"
      "${CMAKE_CURRENT_SOURCE_DIR}/packages/metro-config-qnx/src/index.js"
    COMMENT "Build experimental MarathonOS React Native Metro bundle"
    VERBATIM
  )
  add_custom_target(rnqnx_marathon_playground_bundle
    DEPENDS "${RNQNX_MARATHON_BUNDLE}")

  # This is the same host source, Fabric renderer and Hermes runtime as the
  # development-host playground, with Linux Wayland window metadata.
  add_executable(rnqnx_marathon_host
    "${CMAKE_CURRENT_SOURCE_DIR}/vnext/probes/playground/main.cpp")
  target_link_libraries(rnqnx_marathon_host PRIVATE rnqnx_react_host)
  rnqnx_enable_strict_warnings(rnqnx_marathon_host)
  if(RNQNX_MARATHON_HOST_NITRO)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/native/platform/linux-host/nitro"
      "${CMAKE_BINARY_DIR}/marathon-nitro-runtime")
    target_link_libraries(rnqnx_marathon_host PRIVATE
      rnqnx_nitro_marathon rnqnx_nitro_qt)
    target_compile_definitions(rnqnx_marathon_host PRIVATE
      RNQNX_MARATHON_NITRO=1)
  endif()
  add_dependencies(rnqnx_marathon_host rnqnx_marathon_playground_bundle)
  target_compile_definitions(rnqnx_marathon_host PRIVATE
    RNQNX_MARATHON_HOST=1
    RNQNX_PLAYGROUND_BUNDLE_PATH="${RNQNX_MARATHON_BUNDLE}"
  )
  set_target_properties(rnqnx_marathon_host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()
