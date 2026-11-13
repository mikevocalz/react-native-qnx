include_guard(GLOBAL)

# Development client: the consumer app loads its bundle from Metro instead of
# the staged bytecode, with Fast Refresh, reload and LogBox. OFF by default; a
# release build contains none of it. See docs/getting-started/dev-workflow.md.
option(RNQNX_ENABLE_DEV_SUPPORT
  "Build the development client (Metro bundle, Fast Refresh, reload, LogBox); links a Hermes VM that can compile source"
  OFF)

if(RNQNX_ENABLE_DEV_SUPPORT AND NOT RNQNX_ENABLE_WEBSOCKETS)
  message(FATAL_ERROR
    "RNQNX_ENABLE_DEV_SUPPORT needs RNQNX_ENABLE_WEBSOCKETS=ON: Metro's Fast Refresh "
    "client and its /message channel are WebSockets.")
endif()

# Called after rnqnx_react_host exists.
function(rnqnx_configure_dev_support)
  if(NOT RNQNX_ENABLE_DEV_SUPPORT OR TARGET rnqnx_dev_support)
    return()
  endif()
  find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Network Qml Quick WebSockets)
  add_library(rnqnx_dev_support STATIC
    "${RNQNX_REACT_QT_ROOT}/vnext/src/runtime/QnxDevSupport.cpp"
    "${RNQNX_REACT_QT_ROOT}/vnext/include/react-native-qnx/runtime/QnxDevSupport.h")
  target_link_libraries(rnqnx_dev_support PUBLIC
    rnqnx_react_host Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::WebSockets)
  target_compile_definitions(rnqnx_dev_support PUBLIC RNQNX_ENABLE_DEV_SUPPORT=1)
  set_target_properties(rnqnx_dev_support PROPERTIES AUTOMOC ON)
  rnqnx_enable_strict_warnings(rnqnx_dev_support)
endfunction()
