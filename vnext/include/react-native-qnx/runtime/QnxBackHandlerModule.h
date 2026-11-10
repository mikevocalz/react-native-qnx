#pragma once

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>
#include <memory>

class QWindow;

namespace rnqnx::runtime {

/// RN's internal DeviceEventManager contract, not a new application native API.
/// All methods and destruction run on the QGuiApplication/Hermes owner thread.
class QnxBackHandlerModule final
    : public facebook::react::NativeDeviceEventManagerCxxSpec<QnxBackHandlerModule> {
 public:
  explicit QnxBackHandlerModule(std::shared_ptr<facebook::react::CallInvoker> invoker);
  ~QnxBackHandlerModule() override;

  void invokeDefaultBackPressHandler(facebook::jsi::Runtime& runtime);

  /// Observe only this host's window. Rebinding cancels queued old-window events.
  void setWindow(QWindow* window);
  /// Remove native observation and cancel accepted deliveries before runtime drain.
  void invalidate() noexcept;

 private:
  struct Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace rnqnx::runtime
