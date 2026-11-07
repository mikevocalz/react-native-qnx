#include "QnxNitroRuntime.hpp"
#include "QnxPlatformInfo.hpp"

#include <NitroModules/HybridClassRegistry.hpp>
#include <NitroModules/InstallNitro.hpp>
#include <stdexcept>
#include <utility>

namespace rnqnx::nitro {

void installQnxNitroRuntime(facebook::jsi::Runtime& runtime) {
  // Nitro v1 changed the global registry from HybridObject constructors to
  // immutable HybridClass descriptors, with real JavaScript constructors.
  if (!margelo::nitro::HybridClassRegistry::hasHybridClass("QnxPlatformInfo")) {
    margelo::nitro::HybridClassRegistry::registerHybridClass(
        "QnxPlatformInfo", QnxPlatformInfo::Class::get());
  }

  // This overload intentionally installs no dispatcher. Async hosts must use
  // the owned-dispatcher overload before exposing Promises or callbacks.
  margelo::nitro::install(runtime);
}

void installQnxNitroRuntime(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher) {
  if (!dispatcher) {
    throw std::invalid_argument("QNX async Nitro requires an owned JS dispatcher");
  }
  installQnxNitroRuntime(runtime);
  margelo::nitro::Dispatcher::installRuntimeGlobalDispatcher(
      runtime, std::move(dispatcher));
}

} // namespace rnqnx::nitro
