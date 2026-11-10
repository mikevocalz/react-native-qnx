#include "ExpoQnxModuleRegistry.hpp"
#include <QCoreApplication>
#include <memory>

// Test-only native provider: real Qt process metadata, acquired through the
// production registry. No simulated Expo SDK or replacement host.
extern "C" void rnqnxRegisterContractProvider(rnqnx::expo::ExpoQnxModuleRegistry& registry) {
  registry.registerModule("ExpoAutolinkContract", [](facebook::jsi::Runtime& runtime) {
    namespace jsi = facebook::jsi;
    jsi::Object module(runtime);
    auto calls = std::make_shared<int>(0);
    module.setProperty(runtime, "read", jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forAscii(runtime, "read"), 0,
        [calls](jsi::Runtime& owner, const jsi::Value&, const jsi::Value*, size_t) -> jsi::Value {
          jsi::Object result(owner);
          result.setProperty(owner, "name", jsi::String::createFromUtf8(owner, QCoreApplication::applicationName().toStdString()));
          result.setProperty(owner, "calls", ++*calls);
          return result;
        }));
    return module;
  });
}
