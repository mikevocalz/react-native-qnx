// Asserts the host Hermes VM is not the SlowDebug build.
//
// Hermes reports its build mode as HermesInternal.getRuntimeProperties().Build
// (lib/VM/JSLib/HermesInternal.cpp at the pinned commit): "SlowDebug" when the
// VM was compiled with HERMES_SLOW_DEBUG, "Debug" when NDEBUG is unset and
// "Release" otherwise. SlowDebug runs heap and card-table validation on every
// young-generation collection, which made the SVG corpus shards take minutes
// (docs/validation/pr161-layout-dispatch-2026-11-16.md). cmake/dependencies/
// Hermes.cmake forces HERMES_SLOW_DEBUG off; this test fails if that stops
// reaching the compiled VM. It runs on the host only: the QNX lean VM does not
// compile the compiler or HermesInternal surface this reads.

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <cstdio>
#include <memory>
#include <string>

int main() {
  auto runtime = facebook::hermes::makeHermesRuntime();
  const facebook::jsi::Value result = runtime->evaluateJavaScript(
      std::make_unique<facebook::jsi::StringBuffer>(
          "typeof HermesInternal === 'object' && HermesInternal !== null"
          " ? String(HermesInternal.getRuntimeProperties().Build) : 'no-HermesInternal'"),
      "build-mode");
  if (!result.isString()) {
    std::fprintf(stderr, "RNQNX_HERMES_BUILD_MODE_FAIL result is not a string\n");
    return 1;
  }
  const std::string build = result.getString(*runtime).utf8(*runtime);
  std::printf("RNQNX_HERMES_BUILD_MODE %s\n", build.c_str());
  if (build == "SlowDebug") {
    std::fprintf(
        stderr,
        "RNQNX_HERMES_BUILD_MODE_FAIL the Hermes VM was built with HERMES_SLOW_DEBUG\n");
    return 1;
  }
  if (build != "Debug" && build != "Release") {
    std::fprintf(stderr, "RNQNX_HERMES_BUILD_MODE_FAIL unexpected Build value '%s'\n", build.c_str());
    return 1;
  }
  return 0;
}
