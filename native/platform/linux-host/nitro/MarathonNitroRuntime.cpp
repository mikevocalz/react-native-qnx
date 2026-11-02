#include "MarathonNitroRuntime.hpp"
#include "MarathonPlatformInfo.hpp"

#include <NitroModules/HybridClassRegistry.hpp>
#include <NitroModules/InstallNitro.hpp>

namespace rnqnx::nitro {

void installMarathonNitroRuntime(facebook::jsi::Runtime& runtime) {
  // The registered Class metadata describes the local Linux native type.
  // Do not install QnxPlatformInfo or rely on the QNX Screen adapter.
  if (!margelo::nitro::HybridClassRegistry::hasHybridClass(
          "MarathonPlatformInfo")) {
    margelo::nitro::HybridClassRegistry::registerHybridClass(
        "MarathonPlatformInfo", MarathonPlatformInfo::Class::get());
  }

  // Deliberately no async dispatcher. The actual Qt JS loop must supply an
  // owned dispatcher with cancellation/teardown before callbacks work.
  margelo::nitro::install(runtime);
}

} // namespace rnqnx::nitro
