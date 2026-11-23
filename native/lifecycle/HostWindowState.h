#pragma once

// What the host knows about its own window that Qt's application state does
// not report. Minimizing on QNX Screen destroys the native window (ADR-0016
// item 3, ADR-0024), and Qt does not reliably change applicationState() when
// that happens, so the code that minimizes and restores says so here and
// AppState follows it.

#include <QGuiApplication>
#include <QObject>

#include <atomic>

namespace rnqnx::runtime {

// One per process. Call setMinimized() and attach() on the QGuiApplication
// owner thread (the thread that owns QWindow); connect to minimizedChanged()
// on that thread too. minimized() and qtApplicationState() may be read from
// any thread: with the JavaScript thread on its own thread, AppState's first
// value is read there before its GUI-side observer exists.
class HostWindowState final : public QObject {
  Q_OBJECT

 public:
  static HostWindowState& instance();

  bool minimized() const noexcept { return minimized_.load(std::memory_order_acquire); }
  // Emits minimizedChanged only when the value changes.
  void setMinimized(bool minimized);

  // Mirrors QGuiApplication::applicationState() in an atomic. Idempotent.
  void attach(QGuiApplication& application);
  Qt::ApplicationState qtApplicationState() const noexcept {
    return static_cast<Qt::ApplicationState>(qtState_.load(std::memory_order_acquire));
  }

 Q_SIGNALS:
  void minimizedChanged(bool minimized);

 private:
  HostWindowState() = default;
  std::atomic<bool> minimized_{false};
  std::atomic<int> qtState_{static_cast<int>(Qt::ApplicationInactive)};
  bool attached_{false};
};

}  // namespace rnqnx::runtime
