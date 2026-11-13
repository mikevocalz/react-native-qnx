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
  guiContext_.store(guiContextOwner_.get());
  QObject::connect(window, &QQuickWindow::afterAnimating, guiContextOwner_.get(),
                   [this]() { onAfterAnimating(); }, Qt::DirectConnection);
}

QtFrameBridge::~QtFrameBridge() {
  shutdown();
}

void QtFrameBridge::request(FrameCallback callback) {
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
  // The context is only destroyed by shutdown(), on the GUI thread, after the
  // flag below flips; a post that races it is dropped with the context.
  QObject* context = guiContext_.load(std::memory_order_acquire);
  if (context == nullptr || shutdown_.load(std::memory_order_acquire)) {
    return;
  }
  QMetaObject::invokeMethod(context, [job = std::move(job)]() { job(); }, Qt::QueuedConnection);
}

void QtFrameBridge::onAfterAnimating() {
  signalled_.fetch_add(1, std::memory_order_relaxed);
  if (shutdown_.load(std::memory_order_acquire) || !wanted_.exchange(false)) {
    return;
  }
  const double now =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
  uiThread_.post([this, now]() {
    // Callbacks added while these run belong to the next frame.
    std::vector<FrameCallback> batch;
    batch.swap(pending_);
    for (auto& callback : batch) {
      callback(now);
    }
    delivered_.fetch_add(1, std::memory_order_relaxed);
  });
}

void QtFrameBridge::shutdown() {
  if (shutdown_.exchange(true)) {
    return;
  }
  // Destroying the context disconnects the signal and drops queued updates.
  guiContext_.store(nullptr, std::memory_order_release);
  guiContextOwner_.reset();
}

}  // namespace rnqnx::worklets
