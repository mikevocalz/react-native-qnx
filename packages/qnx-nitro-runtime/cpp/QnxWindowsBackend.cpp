#include "QnxWindowsBackend.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace rnqnx::nitro {

namespace {
std::mutex activeMutex;
std::shared_ptr<QnxWindowsBackend> activeBackend;

[[noreturn]] void fail(const char* code, const std::string& reason) {
  throw std::runtime_error(std::string(code) + ": " + reason);
}

std::exception_ptr failure(const std::string& error) {
  return std::make_exception_ptr(std::runtime_error(error));
}

TitleBarInsetData toInsetData(const window::WindowInset& inset) {
  return TitleBarInsetData{.top = inset.top,
                           .right = inset.right,
                           .bottom = inset.bottom,
                           .left = inset.left,
                           .overlayX = inset.overlayX,
                           .overlayY = inset.overlayY,
                           .overlayWidth = inset.overlayWidth,
                           .overlayHeight = inset.overlayHeight};
}

bool hasCapability(const std::vector<std::string>& capabilities, const char* name) {
  return std::find(capabilities.begin(), capabilities.end(), name) != capabilities.end();
}
}  // namespace

QnxWindowsBackend::QnxWindowsBackend(std::shared_ptr<margelo::nitro::Dispatcher> dispatcher,
                                     std::shared_ptr<WindowServices> services)
    : dispatcher_(std::move(dispatcher)),
      services_(std::move(services)),
      sequencer_([this](std::uint64_t requestId, std::uint64_t epoch, bool allow) {
        if (!disposed_ && services_->settleClose) services_->settleClose(requestId, epoch, allow);
      }) {}

std::shared_ptr<QnxWindowsBackend> QnxWindowsBackend::create(
    std::shared_ptr<margelo::nitro::Dispatcher> dispatcher,
    std::shared_ptr<WindowServices> services) {
  if (!dispatcher || !services || !services->state || !services->epoch) {
    throw std::invalid_argument("QnxWindows needs a dispatcher and complete window services");
  }
  std::shared_ptr<QnxWindowsBackend> backend(
      new QnxWindowsBackend(std::move(dispatcher), std::move(services)));
  backend->connect();
  std::lock_guard<std::mutex> lock(activeMutex);
  activeBackend = backend;
  return backend;
}

std::shared_ptr<QnxWindowsBackend> QnxWindowsBackend::active() {
  std::lock_guard<std::mutex> lock(activeMutex);
  return activeBackend;
}

void QnxWindowsBackend::postToJs(std::function<void()> task) const {
  dispatcher_->runAsync(std::move(task));
}

void QnxWindowsBackend::connect() {
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  WindowEventHandlers handlers;
  // GUI thread: copy what the event says, then hop to the JavaScript thread.
  handlers.onCloseRequest = [weak](const window::CloseRequestInfo& info) {
    auto self = weak.lock();
    if (!self) return;
    self->postToJs([weak, info] {
      auto backend = weak.lock();
      // An old epoch means the runtime that was asked is gone; the GUI side
      // lets that close proceed by itself.
      if (!backend || backend->disposed_ || info.epoch != backend->services_->epoch->load()) return;
      backend->sequencer_.begin(info);
    });
  };
  handlers.onFocusChanged = [weak](bool focused) {
    if (auto self = weak.lock()) self->emitBool(focused, self->services_->epoch->load(), true);
  };
  handlers.onVisibilityChanged = [weak](bool visible) {
    if (auto self = weak.lock()) self->emitBool(visible, self->services_->epoch->load(), false);
  };
  handlers.onInsetChanged = [weak] {
    auto self = weak.lock();
    if (!self) return;
    // Coalesced: at most one inset task is pending, and it reads the latest snapshot.
    if (!self->services_->state->markInsetPending()) return;
    const auto epoch = self->services_->epoch->load();
    self->postToJs([weak, epoch] {
      auto backend = weak.lock();
      if (!backend) return;
      const auto snapshot = backend->services_->state->takeInsetPending();
      if (backend->disposed_ || epoch != backend->services_->epoch->load()) return;
      const auto listeners = backend->inset_;
      for (const auto& entry : listeners) {
        entry.second.callAndForget(snapshot->top, snapshot->right, snapshot->bottom, snapshot->left,
                                   snapshot->overlayX, snapshot->overlayY, snapshot->overlayWidth,
                                   snapshot->overlayHeight);
      }
    });
  };
  if (services_->connect) services_->connect(std::move(handlers));
}

void QnxWindowsBackend::emitBool(bool value, std::uint64_t epoch, bool focus) {
  // Focus and visibility are never coalesced: each post carries the value it saw.
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  postToJs([weak, value, epoch, focus] {
    auto backend = weak.lock();
    if (!backend || backend->disposed_ || epoch != backend->services_->epoch->load()) return;
    // Copy: a listener may unsubscribe while we iterate.
    const auto listeners = focus ? backend->focus_ : backend->visibility_;
    for (const auto& entry : listeners) entry.second.callAndForget(value);
  });
}

void QnxWindowsBackend::teardown() {
  if (disposed_) return;
  disposed_ = true;
  // Drop late guard results and posts first, then release the JS callbacks
  // here on the JS thread, before the runtime goes.
  services_->epoch->fetch_add(1);
  sequencer_.clear();
  focus_.clear();
  visibility_.clear();
  inset_.clear();
  if (services_->runtimeGone) services_->runtimeGone();
  std::lock_guard<std::mutex> lock(activeMutex);
  if (activeBackend.get() == this) activeBackend.reset();
}

void QnxWindowsBackend::requireLive() const {
  if (disposed_) fail("WINDOWS_DISPOSED", "the JavaScript runtime was torn down");
}

void QnxWindowsBackend::requireCapability(const char* capability, const char* what) const {
  if (!hasCapability(services_->capabilities, capability)) {
    fail("WINDOW_ACTION_UNSUPPORTED", std::string(what) + " is not available on this target");
  }
}

WindowStateData QnxWindowsBackend::state() const {
  requireLive();
  const auto snapshot = services_->state->current();
  return WindowStateData{.sequence = static_cast<double>(snapshot->sequence),
                         .title = snapshot->title,
                         .width = snapshot->width,
                         .height = snapshot->height,
                         .x = snapshot->x,
                         .y = snapshot->y,
                         .focused = snapshot->focused,
                         .visible = snapshot->visible,
                         .maximized = snapshot->maximized,
                         .controlsLayout = snapshot->controlsLayout};
}

margelo::nitro::Promise<void> QnxWindowsBackend::setTitle(const std::string& title) {
  requireLive();
  auto promise = margelo::nitro::Promise<void>::pending();
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  services_->setTitle(title, [weak, promise](std::string error) {
    auto self = weak.lock();
    if (!self) return;
    self->postToJs([promise, error] {
      if (error.empty()) promise.resolve();
      else promise.reject(failure(error));
    });
  });
  return promise;
}

margelo::nitro::Promise<void> QnxWindowsBackend::setControlsLayout(const std::string& layout) {
  requireLive();
  if (layout != "macos" && layout != "windows") {
    fail("INVALID_ARGUMENT", "layout must be 'macos' or 'windows'");
  }
  auto promise = margelo::nitro::Promise<void>::pending();
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  services_->setControlsLayout(layout, [weak, promise](std::string error) {
    auto self = weak.lock();
    if (!self) return;
    self->postToJs([promise, error] {
      if (error.empty()) promise.resolve();
      else promise.reject(failure(error));
    });
  });
  return promise;
}

margelo::nitro::Promise<Rect> QnxWindowsBackend::setBounds(const Rect& bounds) {
  requireLive();
  for (const double value : {bounds.x, bounds.y, bounds.width, bounds.height}) {
    if (!std::isfinite(value)) fail("INVALID_ARGUMENT", "bounds must be finite numbers");
  }
  if (bounds.width < 0 || bounds.height < 0) fail("INVALID_ARGUMENT", "bounds size must not be negative");
  requireCapability("move", "setBounds");
  requireCapability("resize", "setBounds");
  auto promise = margelo::nitro::Promise<Rect>::pending();
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  services_->setBounds(bounds, [weak, promise](std::string error, Rect applied) {
    auto self = weak.lock();
    if (!self) return;
    self->postToJs([promise, error, applied]() mutable {
      if (error.empty()) promise.resolve(std::move(applied));
      else promise.reject(failure(error));
    });
  });
  return promise;
}

margelo::nitro::Promise<void> QnxWindowsBackend::minimize() {
  requireLive();
  requireCapability("minimize", "minimize");
  auto promise = margelo::nitro::Promise<void>::pending();
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  services_->minimize([weak, promise](std::string error) {
    auto self = weak.lock();
    if (!self) return;
    self->postToJs([promise, error] {
      if (error.empty()) promise.resolve();
      else promise.reject(failure(error));
    });
  });
  return promise;
}

margelo::nitro::Promise<bool> QnxWindowsBackend::close(bool force) {
  requireLive();
  requireCapability("close", "close");
  auto promise = margelo::nitro::Promise<bool>::pending();
  std::weak_ptr<QnxWindowsBackend> weak = weak_from_this();
  services_->requestClose(force, [weak, promise](bool accepted) {
    auto self = weak.lock();
    if (!self) return;
    self->postToJs([promise, accepted]() mutable { promise.resolve(std::move(accepted)); });
  });
  return promise;
}

std::uint64_t QnxWindowsBackend::addCloseGuard(GuardCallback guard) {
  requireLive();
  const auto id = ++nextId_;
  sequencer_.add(id, [guard = std::move(guard), id](const window::CloseRequestInfo& request) {
    guard.callAndForget(static_cast<double>(request.requestId), static_cast<double>(id),
                        window::closeReasonName(request.reason),
                        request.cancellable, static_cast<double>(request.deadlineMs));
  });
  if (services_->guardAdded) services_->guardAdded(id);
  return id;
}

void QnxWindowsBackend::resolveCloseGuard(std::uint64_t requestId, std::uint64_t guardId,
                                          bool allow) {
  requireLive();
  sequencer_.resolve(requestId, guardId, allow);
}

std::uint64_t QnxWindowsBackend::addFocusListener(BoolCallback listener) {
  requireLive();
  const auto id = ++nextId_;
  focus_.emplace_back(id, std::move(listener));
  return id;
}

std::uint64_t QnxWindowsBackend::addVisibilityListener(BoolCallback listener) {
  requireLive();
  const auto id = ++nextId_;
  visibility_.emplace_back(id, std::move(listener));
  return id;
}

TitleBarInsetData QnxWindowsBackend::titleBarInset() const {
  requireLive();
  return toInsetData(*services_->state->currentInset());
}

std::uint64_t QnxWindowsBackend::addInsetListener(InsetCallback listener) {
  requireLive();
  const auto id = ++nextId_;
  inset_.emplace_back(id, std::move(listener));
  return id;
}

void QnxWindowsBackend::removeListener(std::uint64_t id) {
  if (disposed_) return;  // idempotent after teardown
  sequencer_.remove(id);
  if (services_->guardRemoved) services_->guardRemoved(id);
  const auto matches = [id](const auto& entry) { return entry.first == id; };
  focus_.erase(std::remove_if(focus_.begin(), focus_.end(), matches), focus_.end());
  visibility_.erase(std::remove_if(visibility_.begin(), visibility_.end(), matches),
                    visibility_.end());
  inset_.erase(std::remove_if(inset_.begin(), inset_.end(), matches), inset_.end());
}

}  // namespace rnqnx::nitro
