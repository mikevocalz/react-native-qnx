#include "QnxNitroRuntime.hpp"
#include "QnxPlatformInfo.hpp"

#include <NitroModules/HybridClassRegistry.hpp>
#include <NitroModules/InstallNitro.hpp>

namespace rnqnx::nitro {

void installQnxNitroRuntime(facebook::jsi::Runtime& runtime) {
  // Nitro v1 changed the global registry from HybridObject constructors to
  // immutable HybridClass descriptors, with real JavaScript constructors.
  if (!margelo::nitro::HybridClassRegistry::hasHybridClass("QnxPlatformInfo")) {
    margelo::nitro::HybridClassRegistry::registerHybridClass(
        "QnxPlatformInfo", QnxPlatformInfo::Class::get());
  }

  // Synchronous methods are the only enabled mode. A real Qt/JS dispatcher
  // must be installed before exposing Promises or callbacks in either OS.
  margelo::nitro::install(runtime);
}

} // namespace rnqnx::nitro
