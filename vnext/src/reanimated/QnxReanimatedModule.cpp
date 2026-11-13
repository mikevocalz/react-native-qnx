#include "QnxReanimatedModule.h"

#include <reanimated/Compat/WorkletsApi.h>
#include <reanimated/RuntimeDecorators/RNRuntimeDecorator.h>

#include <react/renderer/core/EventListener.h>

#include <cstdio>
#include <utility>

namespace rnqnx::reanimated {

using facebook::jsi::JSError;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

QnxReanimatedModule::QnxReanimatedModule(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
    runtime::QtReactHost& host,
    std::shared_ptr<worklets::QtWorkletsPlatform> platform)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)),
      host_(host),
      platform_(std::move(platform)) {
  methodMap_["installTurboModule"] = {0, &QnxReanimatedModule::installTurboModule};
}

Value QnxReanimatedModule::installTurboModule(
    Runtime& rt, facebook::react::TurboModule& tm, const Value*, size_t) {
  auto& self = static_cast<QnxReanimatedModule&>(tm);
  if (self.invalidated_) {
    throw JSError(rt, "[Reanimated] The module was invalidated");
  }
  if (self.proxy_) {
    return Value(true);
  }
  self.install(rt);
  return Value(true);
}

void QnxReanimatedModule::install(Runtime& rt) {
  const auto global = rt.global();
  // Reanimated's JavaScript parks Worklets' holders on the global for the
  // duration of this call (NativeReanimated.ts installTurboModule()).
  if (!global.hasProperty(rt, "__UI_WORKLET_RUNTIME_HOLDER") || !global.hasProperty(rt, "__UI_SCHEDULER_HOLDER")) {
    throw JSError(rt, "[Reanimated] react-native-worklets has not started its UI runtime");
  }
  uiRuntime_ = ::worklets::getWorkletRuntimeFromHolder(
      rt, global.getPropertyAsObject(rt, "__UI_WORKLET_RUNTIME_HOLDER"));
  const auto uiScheduler = ::worklets::getUISchedulerFromHolder(
      rt, global.getPropertyAsObject(rt, "__UI_SCHEDULER_HOLDER"));

  auto methods = platformMethods();
  proxy_ = std::make_shared<::reanimated::ReanimatedModuleProxy>(
      uiRuntime_, uiScheduler, rt, platform_->jsCallInvoker(), methods, /*isReducedMotion=*/false);
  // Reanimated's mapper finalizer calls this on the UI-worklet runtime after
  // _updateProps. Flush the registry into Fabric while the proxy is alive;
  // a no-op here leaves useAnimatedStyle updates pending indefinitely.
  methods.maybeFlushUIUpdatesQueueFunction = [weak = std::weak_ptr<::reanimated::ReanimatedModuleProxy>(proxy_)]() {
    if (auto proxy = weak.lock()) proxy->performOperations();
  };
  proxy_->init(methods);
  proxy_->initializeFabric(host_.scheduler().getUIManager());

  // Fabric events (scroll, gesture) feed worklet handlers. The handler runs on
  // the UI thread; the event is copied across, never the JS runtime's objects.
  host_.scheduler().addEventListener(std::make_shared<const facebook::react::EventListener>(
      [weak = std::weak_ptr<::reanimated::ReanimatedModuleProxy>(proxy_),
       platform = platform_](const facebook::react::RawEvent& event) {
        platform->postToUi([weak, event, platform]() {
          if (auto proxy = weak.lock()) {
            proxy->handleRawEvent(event, platform->animationTimestampMs());
          }
        });
        return false;  // never consume: JS handlers still see the event
      }));

  ::reanimated::RNRuntimeDecorator::decorate(
      rt, ::worklets::getJSIRuntimeFromWorkletRuntime(uiRuntime_), proxy_);
}

::reanimated::PlatformDepMethodsHolder QnxReanimatedModule::platformMethods() {
  using namespace ::reanimated;
  auto* host = &host_;
  auto platform = platform_;
  PlatformDepMethodsHolder holder{};
  holder.requestRender = [platform](std::function<void(const double)> callback) {
    platform->requestFrame(std::move(callback));
  };
  // Called on the UI thread with one tag's props. The Qt item is only touched
  // on the GUI thread: the delegate re-posts if needed.
  holder.synchronouslyUpdateUIPropsFunction = [host](const int tag, const folly::dynamic& props) {
    host->delegate().schedulerShouldSynchronouslyUpdateViewOnUIThread(tag, props);
  };
  holder.getAnimationTimestamp = [platform]() { return platform->animationTimestampMs(); };
  holder.registerSensor = [](int, int, int, std::function<void(double[], int)>) {
    std::fprintf(stderr, "[Reanimated] useAnimatedSensor is not supported on QNX\n");
    return -1;
  };
  holder.unregisterSensor = [](int) {};
  holder.setGestureStateFunction = [](int, int) {};
  holder.subscribeForKeyboardEvents = [](std::function<void(int, int)>, bool, bool) {
    std::fprintf(stderr, "[Reanimated] keyboard events are not supported on QNX\n");
    return -1;
  };
  holder.unsubscribeFromKeyboardEvents = [](int) {};
  holder.attachPseudoSelector = [](facebook::react::Tag, PseudoSelector, std::function<void(bool)>) {};
  holder.detachPseudoSelector = [](facebook::react::Tag, PseudoSelector) {};
  return holder;
}

void QnxReanimatedModule::invalidate() {
  if (invalidated_) {
    return;
  }
  invalidated_ = true;
  if (proxy_) {
    proxy_->cleanupSensors();
  }
  proxy_.reset();
  uiRuntime_.reset();
}

}  // namespace rnqnx::reanimated
