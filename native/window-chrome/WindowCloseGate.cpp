#include "WindowCloseGate.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QQuickWindow>

#include <algorithm>
#include <utility>

namespace rnqnx::window {

WindowCloseGate::WindowCloseGate(QQuickWindow& window, QObject* parent)
    : QObject(parent), window_(&window) {
  window.installEventFilter(this);
  // Qt delivers QEvent::Quit to the application, and the application then
  // sends a close event to every window. QCoreApplication::quit() is how
  // `--replace` and the quit socket command end the process; those never ask
  // a guard (the launcher sends SIGTERM after 5 s whatever the guard says).
  if (auto* app = QCoreApplication::instance()) app->installEventFilter(this);
}

WindowCloseGate::~WindowCloseGate() {
  if (window_) window_->removeEventFilter(this);
  if (auto* app = QCoreApplication::instance()) app->removeEventFilter(this);
}

void WindowCloseGate::setRequestHandler(RequestHandler handler) { handler_ = std::move(handler); }

// Guards registered under an older epoch belong to a runtime that is gone.
void WindowCloseGate::syncEpoch() {
  const auto now = epoch_->load();
  if (now == seenEpoch_) return;
  seenEpoch_ = now;
  guards_.clear();
}

void WindowCloseGate::addGuard(std::uint64_t id) {
  syncEpoch();
  if (std::find(guards_.begin(), guards_.end(), id) == guards_.end()) guards_.push_back(id);
}

void WindowCloseGate::removeGuard(std::uint64_t id) {
  guards_.erase(std::remove(guards_.begin(), guards_.end(), id), guards_.end());
}

void WindowCloseGate::setSystemClose(bool cancellable, int deadlineMs) {
  systemCancellable_ = cancellable;
  systemDeadlineMs_ = deadlineMs;
}

void WindowCloseGate::setVisibleProbe(std::function<bool()> visible) {
  visibleProbe_ = std::move(visible);
}

void WindowCloseGate::noteNextClose(CloseReason reason, bool cancellable, int deadlineMs) {
  next_ = Next{reason, cancellable, deadlineMs};
}

bool WindowCloseGate::pendingIsStale() const {
  return pending_ && pending_->epoch != epoch_->load();
}

void WindowCloseGate::begin(CloseReason reason, bool cancellable, int deadlineMs) {
  CloseRequestInfo info;
  info.requestId = ++nextRequestId_;
  info.epoch = epoch_->load();
  info.reason = reason;
  info.cancellable = cancellable;
  info.deadlineMs = deadlineMs;
  info.guardIds = guards_;
  pending_ = info;
  // Copy first: the handler may settle at once.
  handler_(info);
}

bool WindowCloseGate::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Quit && watched == QCoreApplication::instance()) {
    quitting_ = true;
    // A close already waiting on guards ends with the process.
    if (pending_) finishAccepted(false);
    return false;
  }
  if (watched != window_.data() || event->type() != QEvent::Close || bypass_ || quitting_)
    return false;
  const auto next = std::exchange(next_, std::nullopt);
  syncEpoch();
  if (pendingIsStale()) {
    // The runtime that was asked is gone; this close proceeds unguarded.
    finishAccepted(false);
    return false;
  }
  if (pending_) {
    // A close is already waiting on the guards: join it.
    event->ignore();
    return true;
  }
  if (guards_.empty() || !handler_) {
    // No guard: no JavaScript round trip, so a close press during a JS block works.
    return false;
  }
  event->ignore();
  if (next) {
    begin(next->reason, next->cancellable, next->deadlineMs);
  } else {
    // Nobody announced this close, so the platform sent it: the window
    // manager's close message or a native frame button. (QEvent::spontaneous()
    // cannot tell: QWindow::close() delivers through the same path.)
    const bool shown = window_->isVisible() && window_->visibility() != QWindow::Minimized &&
                       (!visibleProbe_ || visibleProbe_());
    begin(CloseReason::System, systemCancellable_ && shown, systemDeadlineMs_);
  }
  return true;
}

void WindowCloseGate::requestClose(bool force, Waiter waiter) {
  syncEpoch();
  if (waiter) waiters_.push_back(std::move(waiter));
  if (force || pendingIsStale()) {
    finishAccepted(true);
    return;
  }
  if (pending_) return;  // joins the pending close
  if (guards_.empty() || !handler_) {
    finishAccepted(true);
    return;
  }
  begin(CloseReason::App, true, -1);
}

void WindowCloseGate::settle(std::uint64_t requestId, std::uint64_t epoch, bool allow) {
  if (!pending_ || pending_->requestId != requestId || epoch != epoch_->load() ||
      pending_->epoch != epoch)
    return;
  if (allow || !pending_->cancellable) {
    finishAccepted(true);
  } else {
    finishCancelled();
  }
}

void WindowCloseGate::finishAccepted(bool closeWindow) {
  pending_.reset();
  auto waiters = std::move(waiters_);
  waiters_.clear();
  // The Promise settles before the native close is posted: the event loop and
  // the JavaScript runtime stop right after.
  for (auto& waiter : waiters) waiter(true);
  if (closeWindow && window_) {
    bypass_ = true;
    window_->close();
    bypass_ = false;
  }
}

void WindowCloseGate::finishCancelled() {
  pending_.reset();
  auto waiters = std::move(waiters_);
  waiters_.clear();
  for (auto& waiter : waiters) waiter(false);
}

void WindowCloseGate::runtimeGone() {
  seenEpoch_ = epoch_->load();
  guards_.clear();
  if (pending_) finishAccepted(true);
}

void WindowCloseGate::resetRuntime() {
  epoch_->fetch_add(1);
  runtimeGone();
}

}  // namespace rnqnx::window
