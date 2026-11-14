#pragma once

#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class QObject;
class QThread;

namespace rnqnx::worklets {

/// The thread that runs the UI Worklet Runtime.
///
/// It is not the Qt GUI thread and not the React Native JS thread (on this host
/// those are the same thread). It runs a Qt event loop only so that other
/// threads can post jobs to it; it never touches a Qt Quick item. Anything that
/// has to change an item goes back to the GUI thread.
class QtUiThread final {
 public:
  explicit QtUiThread(const QString& name);
  ~QtUiThread();

  QtUiThread(const QtUiThread&) = delete;
  QtUiThread& operator=(const QtUiThread&) = delete;

  /// Posts `job` to the UI thread. Any thread. Returns false, and drops the
  /// job, once `stop()` has begun.
  bool post(std::function<void()> job);

  bool isCurrent() const noexcept;
  std::thread::id id() const noexcept { return id_.load(); }

  /// Quits the loop and joins. Idempotent. Never call from the UI thread.
  /// Jobs already queued but not started are discarded.
  void stop();

 private:
  std::unique_ptr<QThread> thread_;
  std::unique_ptr<QObject> context_;
  // Serializes receiver access with closing admission; jobs never run here.
  std::mutex admissionMutex_;
  std::once_flag stopped_;
  std::atomic<bool> accepting_{true};
  std::atomic<std::thread::id> id_{};
};

}  // namespace rnqnx::worklets
