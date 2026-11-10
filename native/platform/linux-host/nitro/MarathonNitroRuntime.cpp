#include "MarathonNitroRuntime.hpp"
#include "MarathonPlatformInfo.hpp"

#include <NitroModules/HybridClassRegistry.hpp>
#include <NitroModules/InstallNitro.hpp>
#include <stdexcept>
#include <utility>

namespace rnqnx::nitro {

void installMarathonNitroRuntime(facebook::jsi::Runtime& runtime) {
  // The registered Class metadata describes the local Linux native type.
  // Do not install QnxPlatformInfo or rely on the QNX Screen adapter.
  if (!margelo::nitro::HybridClassRegistry::hasHybridClass(
          "MarathonPlatformInfo")) {
    margelo::nitro::HybridClassRegistry::registerHybridClass(
        "MarathonPlatformInfo", MarathonPlatformInfo::Class::get());
  }

  // Synchronous hosts can omit a dispatcher; async hosts use the overload.
  margelo::nitro::install(runtime);
}

void installMarathonNitroRuntime(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher) {
  if (!dispatcher) throw std::invalid_argument("Marathon async Nitro requires an owned JS dispatcher");
  installMarathonNitroRuntime(runtime);
  margelo::nitro::Dispatcher::installRuntimeGlobalDispatcher(runtime, std::move(dispatcher));
}

} // namespace rnqnx::nitro
