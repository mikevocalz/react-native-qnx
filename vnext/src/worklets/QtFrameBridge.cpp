#include "react-native-qnx/worklets/QtFrameBridge.h"

#include "react-native-qnx/worklets/QtUiThread.h"

#include <QMetaObject>
#include <QObject>
#include <QQuickWindow>

#include <chrono>
#include <utility>

namespace rnqnx::worklets {

QtFrameBridge::QtFrameBridge(QQuickWindow* window, QtUiThread& uiThread)
    : window_(window), uiThread_(uiThread), guiContextOwner_(std::make_unique<QObject>()) {
  QObject::connect(window, &QQuickWindow::afterAnimating, guiContextOwner_.get(),
                   [this]() { onAfterAnimating(); }, Qt::DirectConnection);
}

QtFrameBridge::~QtFrameBridge() {
  shutdown();
}

void QtFrameBridge::request(FrameCallback callback) {
  if (shutdown_.load(std::memory_order_acquire)) return;
  pending_.push_back(std::move(callback));
  if (shutdown_.load(std::memory_order_acquire) || wanted_.exchange(true)) {
    return;
  }
  // One dirty-mark per outstanding batch. The context object lives on the GUI
  // thread, so the lambda runs there and may touch the window.
  postToGui([window = window_]() {
    if (window) {
      window->update();
    }
  });
}

void QtFrameBridge::postToGui(std::function<void()> job) {
  // An atomic raw pointer does not keep its QObject alive. Hold admission
  // through the queued invocation, then allow shutdown to destroy the receiver.
  std::lock_guard lock(guiAdmissionMutex_);
  if (shutdown_.load(std::memory_order_acquire) || !guiContextOwner_) {
    return;
  }
  QMetaObject::invokeMethod(guiContextOwner_.get(), [job = std::move(job)]() { job(); }, Qt::QueuedConnection);
}

void QtFrameBridge::onAfterAnimating() {
  signalled_.fetch_add(1, std::memory_order_relaxed);
  if (shutdown_.load(std::memory_order_acquire) || !wanted_.exchange(false)) {
    return;
  }
  const double now =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
  uiThread_.post([this, now]() {
    if (shutdown_.load(std::memory_order_acquire)) return;
    // Callbacks added while these run belong to the next frame.
    std::vector<FrameCallback> batch;
    batch.swap(pending_);
    for (auto& callback : batch) {
      if (shutdown_.load(std::memory_order_acquire)) break;
      callback(now);
    }
    delivered_.fetch_add(1, std::memory_order_relaxed);
  });
}

void QtFrameBridge::shutdown() {
  std::unique_ptr<QObject> context;
  {
    std::lock_guard lock(guiAdmissionMutex_);
    if (shutdown_.exchange(true)) return;
    context = std::move(guiContextOwner_);
  }
  // Destroying the context disconnects the signal and drops queued updates.
  // QObject destruction may call other code; keep it outside admission.
  context.reset();
}

}  // namespace rnqnx::worklets
