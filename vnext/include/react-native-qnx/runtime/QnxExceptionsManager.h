#pragma once

#include <FBReactNativeSpec/FBReactNativeSpecJSI.h>

#include <functional>
#include <memory>
#include <string>

namespace rnqnx::runtime {

/// One error React Native reported from JavaScript.
struct JavaScriptError {
  std::string message;
  std::string stack;
  bool isFatal{false};
};

/// React Native's ExceptionsManager, implemented for this platform.
///
/// Without it, every JavaScript error aborts the process: React reaches for
/// `TurboModuleRegistry.getEnforcing('ExceptionsManager')` while reporting, that
/// throws, and the throw escapes. React Native ships implementations only for
/// Apple (`React/CoreModules/RCTExceptionsManager.mm`) and Android.
///
/// This one delegates to the host, which is what ADR-0004 requires of the
/// adapter — it does real work rather than swallowing the report. `dismissRedbox`
/// is a genuine no-op: there is no redbox on this platform, and saying so is
/// more honest than pretending to dismiss one.
class QnxExceptionsManager
    : public facebook::react::NativeExceptionsManagerCxxSpec<
          QnxExceptionsManager> {
 public:
  using Reporter = std::function<void(const JavaScriptError&)>;

  QnxExceptionsManager(
      std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
      Reporter reporter);

  void reportFatalException(
      facebook::jsi::Runtime& runtime,
      facebook::jsi::String message,
      facebook::jsi::Array stack,
      double exceptionId);

  void reportSoftException(
      facebook::jsi::Runtime& runtime,
      facebook::jsi::String message,
      facebook::jsi::Array stack,
      double exceptionId);

  void reportException(
      facebook::jsi::Runtime& runtime,
      facebook::jsi::Object data);

  void dismissRedbox(facebook::jsi::Runtime& runtime);

 private:
  Reporter reporter_;
};

}  // namespace rnqnx::runtime
