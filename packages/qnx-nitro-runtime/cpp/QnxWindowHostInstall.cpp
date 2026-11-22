#include "QnxWindow.hpp"
#include "QtNitroDispatcher.hpp"
#include "react-native-qnx/runtime/QtReactHost.h"
#include <stdexcept>

namespace rnqnx::nitro::windowapi {
void installWindowBindings(facebook::jsi::Runtime& runtime,
    const std::shared_ptr<rnqnx::window::WindowSessionState>& state,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher);

void installWindowHost(rnqnx::runtime::QtReactHost& host,
    const std::shared_ptr<rnqnx::window::WindowSessionState>& state) {
  if (!state) throw std::invalid_argument("Window API requires a native window state");
  host.addTeardownHook([weak = std::weak_ptr(state)] {
    if (const auto live = weak.lock()) live->shutdown();
  });
  host.runOnJsThread([&] {
    installWindowBindings(host.runtime(), state,
        std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host.taskQueue()));
  });
}
} // namespace rnqnx::nitro::windowapi
