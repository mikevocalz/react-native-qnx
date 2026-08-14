// Proves that the Hermes build produces a usable JSI runtime on the development
// host: evaluate JavaScript, read a value back through JSI, and exercise the
// language features React Native's bundles depend on.
//
// The async-arrow case is deliberate. react-native-linux hit a Hermes build
// where `async () => {}` bodies silently did not execute; that landmine is
// asserted here so a regression is a failed build rather than a mystery at
// runtime. See docs/handoff/TEST_STATUS.md.

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <cstdio>
#include <memory>
#include <string>

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

int main() {
  std::unique_ptr<Runtime> runtime = makeHermesRuntime();

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
  // actually run. A microtask drains it before the value is read.
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
  if (!evaluateToInt(*runtime, "doubler(21)", 42, "host-function")) {
    return 1;
  }

  std::printf("RNQNX_HERMES_OK arithmetic es6 async-arrow host-function\n");
  return 0;
}
