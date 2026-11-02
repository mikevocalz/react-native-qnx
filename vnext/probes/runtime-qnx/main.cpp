// Physical-target integration probe for the current QNX runtime seams.
// This intentionally exercises Hermes + Nitro + Expo Modules through JSI in one
// process. It is only a passed QNX gate after this binary executes on the target.

#include "ExpoQnxInstaller.hpp"
#include "QnxNitroRuntime.hpp"

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

using facebook::hermes::makeHermesRuntime;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

const char* expectedArchitecture() {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__x86_64__)
  return "x86_64";
#else
  return "unknown";
#endif
}

bool evaluateToString(
    Runtime& runtime,
    const char* source,
    const std::string& expected,
    const char* label) {
  const Value result = runtime.evaluateJavaScript(
      std::make_unique<facebook::jsi::StringBuffer>(source), label);
  if (!result.isString()) {
    std::fprintf(stderr, "RNQNX_RUNTIME_QNX_FAIL %s: result is not a string\n", label);
    return false;
  }

  const std::string actual = result.getString(runtime).utf8(runtime);
  if (actual != expected) {
    std::fprintf(
        stderr,
        "RNQNX_RUNTIME_QNX_FAIL %s: got '%s', expected '%s'\n",
        label,
        actual.c_str(),
        expected.c_str());
    return false;
  }
  return true;
}

} // namespace

int main() {
  std::unique_ptr<Runtime> runtime = makeHermesRuntime();

  rnqnx::nitro::installQnxNitroRuntime(*runtime);
  rnqnx::expo::installExpoModulesQnx(*runtime);

  const std::string arch = expectedArchitecture();

  if (!evaluateToString(
          *runtime,
          "(() => {"
          "  const PlatformInfo = NitroModulesProxy.getHybridClass('QnxPlatformInfo');"
          "  const platform = new PlatformInfo();"
          "  return platform.platform + '|' + platform.architecture + '|' + platform.add(20, 22);"
          "})()",
          "qnx|" + arch + "|42",
          "nitro")) {
    return 1;
  }

  if (!evaluateToString(
          *runtime,
          "(() => {"
          "  const platform = expo.modules.ExpoQnxPlatform;"
          "  return platform.platform + '|' + platform.architecture;"
          "})()",
          "qnx|" + arch,
          "expo")) {
    return 1;
  }

  std::printf(
      "RNQNX_RUNTIME_QNX_OK hermes nitro expo arch=%s\n",
      arch.c_str());
  return 0;
}
