#pragma once

// What the host knows about its own window that Qt's application state does
// not report. Minimizing on QNX Screen destroys the native window (ADR-0016
// item 3, ADR-0024), and Qt does not reliably change applicationState() when
// that happens, so the code that minimizes and restores says so here and
// AppState follows it.

#include <QGuiApplication>
#include <QObject>

#include <atomic>
#include <mutex>

namespace rnqnx::runtime {

// One per process. The object lives on the QGuiApplication owner thread no
// matter which thread calls instance() first: with the JavaScript thread on
// its own thread, QnxAppStateModule can be the first caller, and a QObject that
// stayed on that thread would queue the applicationStateChanged mirror update
// onto a thread with no event loop. attach() must run on the owner thread and
// throws std::logic_error elsewhere. setMinimized() may be called from any
// thread: the flag changes at once and minimizedChanged() is emitted on the
// owner thread, in the order the flag changed (a change made on the owner
// thread waits behind any foreign-thread change still queued). minimized() and qtApplicationState() may be read from any
// thread: AppState's first value is read on the JavaScript thread before its
// GUI-side observer exists.
//
// instance() needs the QGuiApplication to exist. Before that the object would
// have no owner thread to live on, so it throws std::logic_error.
class HostWindowState final : public QObject {
  Q_OBJECT

 public:
  static HostWindowState& instance();

  bool minimized() const noexcept { return minimized_.load(std::memory_order_acquire); }
  // Emits minimizedChanged (on the owner thread) only when the value changes.
  void setMinimized(bool minimized);

  // Mirrors QGuiApplication::applicationState() in an atomic. Idempotent.
  void attach(QGuiApplication& application);
  Qt::ApplicationState qtApplicationState() const noexcept {
    return static_cast<Qt::ApplicationState>(qtState_.load(std::memory_order_acquire));
  }

 Q_SIGNALS:
  void minimizedChanged(bool minimized);

 private:
  HostWindowState();
  std::atomic<bool> minimized_{false};
  // Orders flag changes against the emissions they queue. Never held while a
  // signal is emitted.
  std::mutex order_;
  std::atomic<int> queued_{0};
  std::atomic<int> qtState_{static_cast<int>(Qt::ApplicationInactive)};
  bool attached_{false};
};

}  // namespace rnqnx::runtime
