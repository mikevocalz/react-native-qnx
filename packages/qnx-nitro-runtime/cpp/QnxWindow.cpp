#include "QnxWindow.hpp"
#include "WindowObserver.hpp"
#include <NitroModules/HybridClassRegistry.hpp>
#include <NitroModules/JSIConverter+NativeState.hpp>
#include <atomic>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rnqnx::nitro::windowapi {
namespace {
std::exception_ptr closedError() {
  return std::make_exception_ptr(std::runtime_error("The native window or handle is closed"));
}
class Subscription final : public WindowSubscriptionSpec {
 public:
  explicit Subscription(std::function<void()> cleanup) : cleanup_(std::move(cleanup)) {}
  ~Subscription() override { remove(); }
  void remove() override {
    if (cleanup_) { auto cleanup = std::exchange(cleanup_, {}); cleanup(); }
  }
  void dispose() override { remove(); }
  bool active() const { return static_cast<bool>(cleanup_); }
 private:
  std::function<void()> cleanup_;
};
class Window final : public WindowSpec {
 public:
  Window(std::shared_ptr<rnqnx::window::WindowSessionState> state,
         std::shared_ptr<margelo::nitro::Dispatcher> dispatcher)
      : state_(std::move(state)), dispatcher_(std::move(dispatcher)),
        active_(std::make_shared<std::atomic<bool>>(true)) {}
  ~Window() override { dispose(); }
  WindowSnapshot getSnapshot() override {
    if (!active_->load()) throw std::runtime_error("The window handle is disposed");
    std::lock_guard lock(state_->mutex);
    if (!state_->owner) throw std::runtime_error("The native window is closed");
    return windowSnapshot(state_->snapshot);
  }
  margelo::nitro::Promise<void> setTitle(std::string value) override {
    return command([value = std::move(value)](auto& owner) { owner.setTitle(QString::fromStdString(value)); });
  }
  margelo::nitro::Promise<void> setSize(double width, double height) override {
    return command([=](auto& owner) { owner.setSize(width, height); });
  }
  margelo::nitro::Promise<void> setPosition(double x, double y) override {
    return command([=](auto& owner) { owner.setPosition(x, y); });
  }
  margelo::nitro::Promise<void> setControlsLayout(std::string layout) override {
    return command([layout = std::move(layout)](auto& owner) { owner.setControlsLayout(QString::fromStdString(layout)); });
  }
  margelo::nitro::Promise<void> minimize() override { return command([](auto& owner) { owner.minimize(); }); }
  margelo::nitro::Promise<void> restore() override { return command([](auto& owner) { owner.restore(); }); }
  margelo::nitro::Promise<void> setVisible(bool visible) override { return command([=](auto& owner) { owner.setVisible(visible); }); }
  margelo::nitro::Promise<void> requestFocus() override { return command([](auto& owner) { owner.requestFocus(); }); }
  margelo::nitro::Promise<bool> cancelCloseRequest(std::string requestId) override {
    auto promise = margelo::nitro::Promise<bool>::pending();
    const auto active = active_;
    if (!active->load() || !state_->submit([promise, active, requestId = std::move(requestId)](auto& owner) {
      if (!active->load()) { promise.reject(closedError()); return; }
      try { promise.resolve(owner.cancelCloseRequest(QString::fromStdString(requestId))); }
      catch (...) { promise.reject(std::current_exception()); }
    }, [promise] { promise.reject(closedError()); })) promise.reject(closedError());
    return promise;
  }
  margelo::nitro::Promise<bool> close() override {
    auto promise = margelo::nitro::Promise<bool>::pending();
    const auto active = active_;
    if (!active->load() || !state_->submit([promise, active](auto& owner) {
      if (!active->load()) { promise.reject(closedError()); return; }
      try { promise.resolve(owner.close()); } catch (...) { promise.reject(std::current_exception()); }
    }, [promise] { promise.reject(closedError()); })) promise.reject(closedError());
    return promise;
  }
  std::shared_ptr<WindowSubscriptionSpec> addSnapshotListener(margelo::nitro::Callback<void(WindowSnapshot)> callback) override {
    (void)getSnapshot();
    auto observer = std::make_shared<WindowObserver>(state_, active_, std::move(callback), dispatcher_);
    const auto id = state_->listen([weak = std::weak_ptr(observer)](auto version, const auto& value, bool initial) {
      if (auto live = weak.lock()) live->enqueue(version, value, initial);
    });
    return retain([state = state_, id, observer] { observer->stop(); state->unlisten(id); });
  }
  std::shared_ptr<WindowSubscriptionSpec> addCloseGuard() override {
    (void)getSnapshot();
    const auto active = state_->guard();
    return retain([active] { active->store(false); });
  }
  void dispose() override {
    if (!active_->exchange(false)) return;
    for (auto& subscription : subscriptions_) subscription->remove();
    subscriptions_.clear();
  }
 private:
  margelo::nitro::Promise<void> command(std::function<void(rnqnx::window::WindowSession&)> action) {
    auto promise = margelo::nitro::Promise<void>::pending();
    const auto active = active_;
    if (!active->load() || !state_->submit([promise, active, action = std::move(action)](auto& owner) {
      if (!active->load()) { promise.reject(closedError()); return; }
      try { action(owner); promise.resolve(); } catch (...) { promise.reject(std::current_exception()); }
    }, [promise] { promise.reject(closedError()); })) promise.reject(closedError());
    return promise;
  }
  std::shared_ptr<WindowSubscriptionSpec> retain(std::function<void()> cleanup) {
    subscriptions_.erase(std::remove_if(subscriptions_.begin(), subscriptions_.end(),
        [](const auto& item) { return !item->active(); }), subscriptions_.end());
    auto subscription = std::make_shared<Subscription>(std::move(cleanup));
    subscriptions_.push_back(subscription);
    return subscription;
  }
  std::shared_ptr<rnqnx::window::WindowSessionState> state_;
  std::shared_ptr<margelo::nitro::Dispatcher> dispatcher_;
  std::shared_ptr<std::atomic<bool>> active_;
  std::vector<std::shared_ptr<Subscription>> subscriptions_;
};
class WindowHost final : public WindowHostSpec {
 public:
  WindowHost(std::shared_ptr<rnqnx::window::WindowSessionState> state,
             std::shared_ptr<margelo::nitro::Dispatcher> dispatcher)
      : state_(std::move(state)), dispatcher_(std::move(dispatcher)) {}
  std::shared_ptr<WindowSpec> getCurrentWindow() override {
    { std::lock_guard lock(state_->mutex);
      if (!state_->owner) throw std::runtime_error("The native window is closed"); }
    return std::make_shared<Window>(state_, dispatcher_);
  }
 private:
  std::shared_ptr<rnqnx::window::WindowSessionState> state_;
  std::shared_ptr<margelo::nitro::Dispatcher> dispatcher_;
};
} // namespace
void installWindowBindings(facebook::jsi::Runtime& runtime,
    const std::shared_ptr<rnqnx::window::WindowSessionState>& state,
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher) {
  if (!state || !dispatcher || !dispatcher->isOnDispatcher())
    throw std::invalid_argument("Window API requires an installed owner dispatcher and window state");
  if (!runtime.global().getProperty(runtime, "__rnqnxWindowHost").isUndefined())
    throw std::runtime_error("The runtime already has a window host");
  for (const auto& cls : {WindowSpec::Class::get(), WindowSubscriptionSpec::Class::get(), WindowHostSpec::Class::get()})
    if (!margelo::nitro::HybridClassRegistry::hasHybridClass(cls->getName()))
      margelo::nitro::HybridClassRegistry::registerHybridClass(cls->getName(), cls);
  std::shared_ptr<WindowHostSpec> host = std::make_shared<WindowHost>(state, std::move(dispatcher));
  runtime.global().setProperty(runtime, "__rnqnxWindowHost",
      margelo::nitro::JSIConverter<std::shared_ptr<WindowHostSpec>>::toJSI(runtime, host));
}
} // namespace rnqnx::nitro::windowapi
