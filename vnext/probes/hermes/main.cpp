// Proves that the Hermes build produces a usable JSI runtime on the host or
// QNX target: evaluate JavaScript, read a value back through JSI, and exercise the
// language features React Native's bundles depend on.
//
// The async-arrow case is deliberate. react-native-linux hit a Hermes build
// where `async () => {}` bodies silently did not execute; that landmine is
// asserted here so a regression is a failed build rather than a mystery at
// runtime. QNX lean builds consume the compiled bytecode-smoke.js instead of
// compiling source. See docs/handoff/TEST_STATUS.md.

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#if defined(__QNXNTO__)
#include <pthread.h>
#endif

namespace {

using facebook::hermes::makeHermesRuntime;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

bool evaluateToInt(Runtime& runtime, const char* source, int expected, const char* label) {
  const Value result = runtime.evaluateJavaScript(
      std::make_unique<facebook::jsi::StringBuffer>(source), label);
  if (!result.isNumber()) {
    std::fprintf(stderr, "RNQNX_HERMES_FAIL %s: result is not a number\n", label);
    return false;
  }
  const int actual = static_cast<int>(result.getNumber());
  if (actual != expected) {
    std::fprintf(
        stderr, "RNQNX_HERMES_FAIL %s: got %d, expected %d\n", label, actual, expected);
    return false;
  }
  return true;
}

}  // namespace

static int runProbe(const char* bytecodePath) {
  std::unique_ptr<Runtime> runtime = makeHermesRuntime();

  // A host function crossing the JSI boundary in both directions.
  auto doubler = facebook::jsi::Function::createFromHostFunction(
      *runtime,
      facebook::jsi::PropNameID::forAscii(*runtime, "doubler"),
      1,
      [](Runtime&, const Value&, const Value* args, size_t count) -> Value {
        if (count != 1 || !args[0].isNumber()) {
          return Value::undefined();
        }
        return Value(args[0].getNumber() * 2);
      });
  runtime->global().setProperty(*runtime, "doubler", doubler);

  if (bytecodePath != nullptr) {
    std::ifstream input(bytecodePath, std::ios::binary);
    if (!input) {
      std::fprintf(stderr, "RNQNX_HERMES_FAIL cannot open bytecode file\n");
      return 1;
    }
    std::string bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (bytes.empty() || input.bad()) {
      std::fprintf(stderr, "RNQNX_HERMES_FAIL cannot read bytecode file\n");
      return 1;
    }
    const Value result = runtime->evaluateJavaScript(
        std::make_unique<facebook::jsi::StringBuffer>(std::move(bytes)), bytecodePath);
    if (!result.isNumber() || result.getNumber() != 42) {
      std::fprintf(stderr, "RNQNX_HERMES_FAIL bytecode result\n");
      return 1;
    }
    return 0;
  }

  if (!evaluateToInt(*runtime, "1 + 1", 2, "arithmetic")) {
    return 1;
  }

  // Closures, spread, destructuring, template literals, classes: the shapes a
  // React Native bundle is full of.
  if (!evaluateToInt(
          *runtime,
          "(() => {"
          "  class Counter { constructor(n) { this.n = n; } add(...xs) {"
          "    return xs.reduce((a, b) => a + b, this.n); } }"
          "  const { n } = new Counter(10);"
          "  return new Counter(n).add(1, 2, 3);"
          "})()",
          16,
          "es6")) {
    return 1;
  }

  // The react-native-linux landmine: an async arrow function whose body must
  // actually run before its first suspension.
  if (!evaluateToInt(
          *runtime,
          "(() => {"
          "  let seen = 0;"
          "  const bump = async () => { seen = 7; };"
          "  bump();"
          "  return seen;"
          "})()",
          7,
          "async-arrow")) {
    return 1;
  }

  if (!evaluateToInt(*runtime, "doubler(21)", 42, "host-function")) {
    return 1;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    std::fprintf(stderr, "Usage: %s [probe.hbc]\n", argv[0]);
    return 2;
  }
  const char* bytecodePath = argc == 2 ? argv[1] : nullptr;
#if defined(__QNXNTO__)
  if (bytecodePath == nullptr) {
    std::fprintf(stderr, "QNX lean Hermes requires the compiled bytecode-smoke.js probe.\n");
    return 2;
  }
#endif
  if (const int result = runProbe(bytecodePath)) {
    return result;
  }
#if defined(__QNXNTO__)
  // Exercise actual thread stack bounds, including stacks that differ from
  // the process default. A nested-frame approximation falsely overflows here.
  for (const size_t stackSize : {256u * 1024u, 1024u * 1024u}) {
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
      return 1;
    }
    if (pthread_attr_setstacksize(&attr, stackSize) != 0) {
      pthread_attr_destroy(&attr);
      return 1;
    }
    struct ThreadProbe {
      const char* path;
      int result;
    } output{bytecodePath, 1};
    pthread_t thread;
    const int created = pthread_create(
        &thread, &attr,
        [](void* rawOutput) -> void* {
          auto* probe = static_cast<ThreadProbe*>(rawOutput);
          probe->result = runProbe(probe->path);
          return nullptr;
        },
        &output);
    pthread_attr_destroy(&attr);
    if (created != 0 || pthread_join(thread, nullptr) != 0 || output.result != 0) {
      std::fprintf(stderr, "RNQNX_HERMES_FAIL thread stack=%zu\n", stackSize);
      return 1;
    }
    std::printf("RNQNX_HERMES_QNX_STACK_OK bytes=%zu\n", stackSize);
  }
#endif
  std::printf("RNQNX_HERMES_OK %s\n",
              bytecodePath ? "bytecode" : "arithmetic es6 async-arrow host-function");
  return 0;
}
