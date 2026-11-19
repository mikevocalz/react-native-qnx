#include "QnxWorkletsModule.h"

#include <worklets/WorkletRuntime/WorkletRuntime.h>

#include <stdexcept>
#include <utility>

namespace rnqnx::worklets {

using facebook::jsi::JSError;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

QnxWorkletsModule::QnxWorkletsModule(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
    std::shared_ptr<::worklets::WorkletsModuleProxyInitializer> initializer,
    UiRuntimeReady onUiRuntimeReady)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)),
      initializer_(std::move(initializer)),
      onUiRuntimeReady_(std::move(onUiRuntimeReady)) {
  methodMap_["installTurboModule"] = {1, &QnxWorkletsModule::installTurboModule};
  methodMap_["prepareBundleMode"] = {0, &QnxWorkletsModule::prepareBundleMode};
  methodMap_["toggleSlowAnimationsOnUIRuntime"] = {0, &QnxWorkletsModule::toggleSlowAnimationsOnUIRuntime};
  methodMap_["start"] = {0, &QnxWorkletsModule::start};
}

void QnxWorkletsModule::invalidate() {
  if (invalidated_) {
    return;
  }
  invalidated_ = true;
  if (initializer_) {
    initializer_->invalidate();
  }
  proxy_.reset();
  initializer_.reset();
}

Value QnxWorkletsModule::installTurboModule(
    Runtime& rt, facebook::react::TurboModule& tm, const Value* args, size_t count) {
  auto& self = static_cast<QnxWorkletsModule&>(tm);
  if (self.invalidated_ || !self.initializer_) {
    throw JSError(rt, "[Worklets] The module was invalidated");
  }
  const bool bundleMode = count >= 1 && args[0].isBool() && args[0].getBool();
  if (bundleMode) {
    // The unpacker precedence and the Metro serializer are separate work; see
    // docs/worklets-qnx.md.
    throw JSError(rt, "[Worklets] Bundle Mode is not supported on QNX");
  }
  self.proxy_ = self.initializer_->finalize(rt, false, []() -> ::worklets::BundleModeConfig {
    throw std::logic_error("Bundle Mode is not supported");
  });
  return Value(true);
}

Value QnxWorkletsModule::prepareBundleMode(
    Runtime& rt, facebook::react::TurboModule&, const Value*, size_t) {
  throw JSError(rt, "[Worklets] Bundle Mode is not supported on QNX");
}

Value QnxWorkletsModule::toggleSlowAnimationsOnUIRuntime(
    Runtime&, facebook::react::TurboModule&, const Value*, size_t) {
  // A development aid on iOS (simulator "slow animations"). Nothing to toggle.
  return Value(false);
}

Value QnxWorkletsModule::start(
    Runtime& rt, facebook::react::TurboModule& tm, const Value*, size_t) {
  auto& self = static_cast<QnxWorkletsModule&>(tm);
  if (!self.proxy_) {
    throw JSError(rt, "[Worklets] start() called before installTurboModule()");
  }
  self.proxy_->start();
  if (self.onUiRuntimeReady_) {
    self.onUiRuntimeReady_(rt);  // the RN runtime gets the same diagnostics
    const auto uiRuntime = self.proxy_->getUIWorkletRuntime();
    uiRuntime->runSync([&self](Runtime& uiRt) { self.onUiRuntimeReady_(uiRt); });
  }
  return Value(true);
}

}  // namespace rnqnx::worklets
