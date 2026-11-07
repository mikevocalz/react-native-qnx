include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/QnxSourcePatches.cmake)

# Preserve lifetime annotations only on compilers that implement them.
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/NitroDefines.hpp"
  "#define NitroDefines_h"
  "#define NitroDefines_h\n\n#if defined(__clang__)\n#define RNQNX_NITRO_LIFETIMEBOUND [[clang::lifetimebound]]\n#else\n#define RNQNX_NITRO_LIFETIMEBOUND\n#endif")
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/ArrayBuffer.hpp"
  "uint8_t* NULLABLE data() const [[clang::lifetimebound]];\n  size_t size() const;"
  "uint8_t* NULLABLE data() const RNQNX_NITRO_LIFETIMEBOUND;\n  size_t size() const;")
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/ArrayBuffer.hpp"
  "uint8_t* NULLABLE data() const [[clang::lifetimebound]];\n  size_t size() const SWIFT_PRIVATE;"
  "uint8_t* NULLABLE data() const RNQNX_NITRO_LIFETIMEBOUND;\n  size_t size() const SWIFT_PRIVATE;")
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/InlineCallback.hpp"
  "jsi::Runtime& runtime [[clang::lifetimebound]], jsi::Function&& function"
  "jsi::Runtime& runtime RNQNX_NITRO_LIFETIMEBOUND, jsi::Function&& function")

# Clang diagnostic directives are not GCC directives.
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/NitroLogger.hpp"
  "#pragma clang diagnostic push"
  "#if defined(__clang__)\n#pragma clang diagnostic push\n#endif")
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/NitroLogger.hpp"
  "#pragma clang diagnostic ignored \"-Wformat-security\""
  "#if defined(__clang__)\n#pragma clang diagnostic ignored \"-Wformat-security\"\n#endif")
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/NitroLogger.hpp"
  "#pragma clang diagnostic pop"
  "#if defined(__clang__)\n#pragma clang diagnostic pop\n#endif")

# Use the standard implicit move for the local converted ArrayBuffer.
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/JSIConverter+ArrayBuffer.hpp"
  "return std::move(arrayBuffer);"
  "return arrayBuffer;")

# All valid known global names return; invalid enum values fail closed.
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/src/CommonGlobals.cpp"
  "#include <NitroModules/CommonGlobals.hpp>"
  "#include <NitroModules/CommonGlobals.hpp>\n#include <cstdlib>")
rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/src/CommonGlobals.cpp"
  "      return \"NitroModulesProxy\";\n  }\n}"
  "      return \"NitroModulesProxy\";\n  }\n  std::abort();\n}")

rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/HybridFunction.hpp"
  "#include <exception>"
  "#include <exception>\n#include <cstdlib>")

rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/HybridFunction.hpp"
  "        return typeName + \".\" + registrationName;\n    }\n  }"
  "        return typeName + \".\" + registrationName;\n    }\n    std::abort();\n  }")

rnqnx_patch_qnx_source(
  "${RNQNX_NITRO_CORE_ROOT}/native/cpp/include/NitroModules/HybridFunction.hpp"
  "        return \"set hybrid property `\" + funcName + \"`\";\n    }\n  }"
  "        return \"set hybrid property `\" + funcName + \"`\";\n    }\n    std::abort();\n  }")
