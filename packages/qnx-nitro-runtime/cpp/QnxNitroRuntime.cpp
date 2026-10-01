#include "QnxNitroRuntime.hpp"

#include "QnxPlatformInfo.hpp"

#include "HybridObjectRegistry.hpp"
#include "InstallNitro.hpp"
#include <memory>

namespace rnqnx::nitro {

void installQnxNitroRuntime(facebook::jsi::Runtime& runtime) {
  if (!margelo::nitro::HybridObjectRegistry::hasHybridObject("QnxPlatformInfo")) {
    margelo::nitro::HybridObjectRegistry::registerHybridObjectConstructor(
        "QnxPlatformInfo",
        []() -> std::shared_ptr<margelo::nitro::HybridObject> {
          return std::make_shared<QnxPlatformInfo>();
        });
  }

  // Start with Nitro's documented synchronous installer. A dispatcher is not
  // invented here; callbacks/Promises remain disabled until the QNX runtime
  // wires Nitro to the actual JS/Qt event-loop dispatcher.
  margelo::nitro::install(runtime);
}

} // namespace rnqnx::nitro
