// Physical-target integration probe for the current QNX runtime seams.
// This intentionally exercises Hermes + Nitro + Expo Modules through JSI in one
// process. It is only a passed QNX gate after this binary executes on the target.

#include "ExpoQnxInstaller.hpp"
#include "QnxNitroRuntime.hpp"

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

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

std::string readBytecodeFile(const char* file) {
  std::ifstream input(file, std::ios::binary | std::ios::ate);
  const auto size = static_cast<std::streamsize>(input.tellg());
  if (!input || size <= 0 || size > 1024 * 1024) {
    throw std::runtime_error("Expected a nonempty bytecode file up to 1 MiB");
  }
  std::string bytes(static_cast<std::size_t>(size), '\0');
  input.seekg(0);
  if (!input.read(bytes.data(), size)) {
    throw std::runtime_error("Could not read the complete bytecode file");
  }
  return bytes;
}

bool evaluateToString(
    Runtime& runtime,
    const char* file,
    const std::string& expected,
    const char* label) {
  auto buffer = std::make_unique<facebook::jsi::StringBuffer>(readBytecodeFile(file));
  auto* hermes = facebook::jsi::castInterface<facebook::hermes::IHermesRootAPI>(
      facebook::hermes::makeHermesRootAPI());
  if (hermes == nullptr || !hermes->isHermesBytecode(buffer->data(), buffer->size())) {
    throw std::runtime_error("Runtime probe requires Hermes bytecode, not JavaScript source");
  }
  std::string error;
  if (!hermes->hermesBytecodeSanityCheck(buffer->data(), buffer->size(), &error)) {
    throw std::runtime_error("Invalid runtime probe bytecode: " + error);
  }
  const Value result = runtime.evaluateJavaScript(
      std::move(buffer), label);
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

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "Usage: rnqnx_runtime_qnx_probe <nitro.hbc> <expo.hbc>\n");
    return 2;
  }
  try {
    std::unique_ptr<Runtime> runtime = makeHermesRuntime();
    rnqnx::nitro::installQnxNitroRuntime(*runtime);
    rnqnx::expo::installExpoModulesQnx(*runtime);
    const std::string arch = expectedArchitecture();

    if (!evaluateToString(*runtime, argv[1], "qnx|" + arch + "|42", "nitro") ||
        !evaluateToString(*runtime, argv[2], "qnx|" + arch, "expo")) {
      return 1;
    }
    std::printf(
        "RNQNX_RUNTIME_QNX_OK hermes nitro expo arch=%s\n",
        arch.c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "RNQNX_RUNTIME_QNX_FAIL %s\n", error.what());
    return 1;
  }
}
