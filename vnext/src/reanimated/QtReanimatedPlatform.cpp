#include "react-native-qnx/reanimated/QtReanimatedPlatform.h"

#include "QnxReanimatedModule.h"

namespace rnqnx::reanimated {

void installReanimated(
    runtime::QtReactHost& host,
    const std::shared_ptr<worklets::QtWorkletsPlatform>& worklets) {
  auto module = std::make_shared<std::shared_ptr<QnxReanimatedModule>>();
  host.registerTurboModule(
      QnxReanimatedModule::kModuleName,
      [module, &host, worklets](std::shared_ptr<facebook::react::CallInvoker>) {
        if (!*module) {
          *module = std::make_shared<QnxReanimatedModule>(worklets->jsCallInvoker(), host, worklets);
        }
        return std::static_pointer_cast<facebook::react::TurboModule>(*module);
      });
  // Runs before the Worklets hook (reverse registration order), so the proxy is
  // gone before the UI thread stops.
  host.addTeardownHook([module]() {
    if (*module) {
      (*module)->invalidate();
    }
  });
}

}  // namespace rnqnx::reanimated
