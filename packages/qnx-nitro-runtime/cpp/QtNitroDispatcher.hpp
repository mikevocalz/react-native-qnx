#pragma once

#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"

#include <NitroModules/Dispatcher.hpp>

namespace rnqnx::nitro {

/// Nitro v1 dispatcher backed by the existing Qt/RN runtime task boundary.
/// Synchronous invocation is owner-thread only; worker completions use runAsync.
class QtNitroDispatcher final : public margelo::nitro::Dispatcher {
 public:
  explicit QtNitroDispatcher(
      std::shared_ptr<runtime::QtRuntimeTaskQueue> queue);
  void runSync(std::function<void()>&& function) override;
  void runAsync(std::function<void()>&& function) override;
  bool isOnDispatcher() override;
  void enterJS(facebook::jsi::Runtime&) override;
  void leaveJS(facebook::jsi::Runtime&) noexcept override;

 private:
  std::shared_ptr<runtime::QtRuntimeTaskQueue> queue_;
};

}  // namespace rnqnx::nitro
