#pragma once

#include <ReactCommon/CallInvoker.h>
#include <ReactCommon/TurboModule.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rnqnx::runtime {

/// What React Native's development JavaScript asks the host to do. The
/// development client implements it; the TurboModules below only translate
/// calls. Every method is called on the JavaScript thread, inside a JS task, so
/// an implementation must not tear down the runtime synchronously: queue the
/// work instead (QnxDevSupport does).
class DevSupportDelegate {
 public:
  virtual ~DevSupportDelegate() = default;

  /// DevSettings.reload / reloadWithReason.
  virtual void reload(const std::string& reason) = 0;
  /// DevSettings.setHotLoadingEnabled, called by HMRClient.enable/disable.
  virtual void setHotLoadingEnabled(bool enabled) = 0;
  /// DevSettings.addMenuItem: a title for the developer menu. Pressing it
  /// emits `didPressMenuItem` through RCTDeviceEventEmitter.
  virtual void addMenuItem(const std::string& title) = 0;
  /// DevLoadingView.showMessage ("Refreshing...", "Loading from Metro...").
  /// Colors are processed React Native colors (0xAARRGGBB) when given.
  virtual void showMessage(
      const std::string& message,
      std::optional<std::uint32_t> textColor,
      std::optional<std::uint32_t> backgroundColor) = 0;
  /// DevLoadingView.hide.
  virtual void hideMessage() = 0;
  /// LogBox.show / LogBox.hide: run or stop the "LogBox" surface over the app.
  virtual void showLogBox() = 0;
  virtual void hideLogBox() = 0;
};

/// The development modules React Native's JavaScript resolves by name:
/// "DevSettings" (getEnforcing in a development bundle), "DevLoadingView",
/// "LogBox" and "RedBox". Returns nullptr for any other name so the host falls
/// through to its own modules. The delegate is held weakly: a module outliving
/// the development client turns into a no-op rather than a dangling call.
std::shared_ptr<facebook::react::TurboModule> makeDevSupportModule(
    const std::string& name,
    const std::shared_ptr<facebook::react::CallInvoker>& jsInvoker,
    std::weak_ptr<DevSupportDelegate> delegate);

}  // namespace rnqnx::runtime
