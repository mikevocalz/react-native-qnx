#include "react-native-qnx/runtime/QnxDevSupportModules.h"

#include <cmath>
#include <utility>

namespace rnqnx::runtime {
namespace {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

/// Shared base: holds the delegate and gives the static method thunks a way
/// back to it. Method tables follow the specs in
/// react-native/src/private/specs_DEPRECATED/modules/Native{DevSettings,
/// DevLoadingView,LogBox,RedBox}.js for React Native 0.88.1.
class DevModule : public react::TurboModule {
 public:
  DevModule(
      const std::string& name,
      std::shared_ptr<react::CallInvoker> jsInvoker,
      std::weak_ptr<DevSupportDelegate> delegate)
      : react::TurboModule(name, std::move(jsInvoker)),
        delegate_(std::move(delegate)) {}

  std::shared_ptr<DevSupportDelegate> delegate() const {
    return delegate_.lock();
  }

 protected:
  using Method = jsi::Value (*)(
      jsi::Runtime&, react::TurboModule&, const jsi::Value*, size_t);
  void add(const char* name, size_t arity, Method method) {
    methodMap_[name] = {arity, method};
  }

 private:
  std::weak_ptr<DevSupportDelegate> delegate_;
};

std::shared_ptr<DevSupportDelegate> delegateOf(react::TurboModule& module) {
  return static_cast<DevModule&>(module).delegate();
}

std::string stringArg(
    jsi::Runtime& rt, const jsi::Value* args, size_t count, size_t index) {
  return index < count && args[index].isString()
      ? args[index].asString(rt).utf8(rt)
      : std::string();
}

std::optional<std::uint32_t> colorArg(
    const jsi::Value* args, size_t count, size_t index) {
  if (index >= count || !args[index].isNumber()) {
    return std::nullopt;
  }
  const double value = args[index].getNumber();
  if (!std::isfinite(value) || value < 0 || value > 4294967295.0) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}

jsi::Value noop(jsi::Runtime&, react::TurboModule&, const jsi::Value*, size_t) {
  return jsi::Value::undefined();
}

class DevSettingsModule final : public DevModule {
 public:
  DevSettingsModule(
      std::shared_ptr<react::CallInvoker> jsInvoker,
      std::weak_ptr<DevSupportDelegate> delegate)
      : DevModule("DevSettings", std::move(jsInvoker), std::move(delegate)) {
    add("reload", 0, [](jsi::Runtime&, react::TurboModule& m, const jsi::Value*, size_t) {
      if (auto d = delegateOf(m)) d->reload("DevSettings.reload");
      return jsi::Value::undefined();
    });
    add("reloadWithReason", 1,
        [](jsi::Runtime& rt, react::TurboModule& m, const jsi::Value* a, size_t c) {
          if (auto d = delegateOf(m)) d->reload(stringArg(rt, a, c, 0));
          return jsi::Value::undefined();
        });
    add("onFastRefresh", 0, noop);
    add("setHotLoadingEnabled", 1,
        [](jsi::Runtime&, react::TurboModule& m, const jsi::Value* a, size_t c) {
          if (auto d = delegateOf(m)) d->setHotLoadingEnabled(c > 0 && a[0].isBool() && a[0].getBool());
          return jsi::Value::undefined();
        });
    // No sampling profiler or element inspector on this target yet; the calls
    // are accepted so React Native's developer menu code does not throw.
    add("setProfilingEnabled", 1, noop);
    add("toggleElementInspector", 0, noop);
    add("addMenuItem", 1,
        [](jsi::Runtime& rt, react::TurboModule& m, const jsi::Value* a, size_t c) {
          if (auto d = delegateOf(m)) d->addMenuItem(stringArg(rt, a, c, 0));
          return jsi::Value::undefined();
        });
    add("openDebugger", 0, noop);
    // NativeEventEmitter bookkeeping. On platforms other than iOS, DevSettings
    // listens through RCTDeviceEventEmitter, which the host calls directly.
    add("addListener", 1, noop);
    add("removeListeners", 1, noop);
    add("setIsShakeToShowDevMenuEnabled", 1, noop);
  }
};

class DevLoadingViewModule final : public DevModule {
 public:
  DevLoadingViewModule(
      std::shared_ptr<react::CallInvoker> jsInvoker,
      std::weak_ptr<DevSupportDelegate> delegate)
      : DevModule("DevLoadingView", std::move(jsInvoker), std::move(delegate)) {
    add("showMessage", 4,
        [](jsi::Runtime& rt, react::TurboModule& m, const jsi::Value* a, size_t c) {
          if (auto d = delegateOf(m)) {
            d->showMessage(stringArg(rt, a, c, 0), colorArg(a, c, 1), colorArg(a, c, 2));
          }
          return jsi::Value::undefined();
        });
    add("hide", 0, [](jsi::Runtime&, react::TurboModule& m, const jsi::Value*, size_t) {
      if (auto d = delegateOf(m)) d->hideMessage();
      return jsi::Value::undefined();
    });
  }
};

class LogBoxModule final : public DevModule {
 public:
  LogBoxModule(
      std::shared_ptr<react::CallInvoker> jsInvoker,
      std::weak_ptr<DevSupportDelegate> delegate)
      : DevModule("LogBox", std::move(jsInvoker), std::move(delegate)) {
    add("show", 0, [](jsi::Runtime&, react::TurboModule& m, const jsi::Value*, size_t) {
      if (auto d = delegateOf(m)) d->showLogBox();
      return jsi::Value::undefined();
    });
    add("hide", 0, [](jsi::Runtime&, react::TurboModule& m, const jsi::Value*, size_t) {
      if (auto d = delegateOf(m)) d->hideLogBox();
      return jsi::Value::undefined();
    });
  }
};

/// HMRClient dismisses the native red box before applying an update. LogBox
/// is the only error UI here, so there is nothing else to dismiss.
class RedBoxModule final : public DevModule {
 public:
  RedBoxModule(
      std::shared_ptr<react::CallInvoker> jsInvoker,
      std::weak_ptr<DevSupportDelegate> delegate)
      : DevModule("RedBox", std::move(jsInvoker), std::move(delegate)) {
    add("setExtraData", 2, noop);
    add("dismiss", 0, noop);
  }
};

}  // namespace

std::shared_ptr<facebook::react::TurboModule> makeDevSupportModule(
    const std::string& name,
    const std::shared_ptr<facebook::react::CallInvoker>& jsInvoker,
    std::weak_ptr<DevSupportDelegate> delegate) {
  if (name == "DevSettings") {
    return std::make_shared<DevSettingsModule>(jsInvoker, std::move(delegate));
  }
  if (name == "DevLoadingView") {
    return std::make_shared<DevLoadingViewModule>(jsInvoker, std::move(delegate));
  }
  if (name == "LogBox") {
    return std::make_shared<LogBoxModule>(jsInvoker, std::move(delegate));
  }
  if (name == "RedBox") {
    return std::make_shared<RedBoxModule>(jsInvoker, std::move(delegate));
  }
  return nullptr;
}

}  // namespace rnqnx::runtime
