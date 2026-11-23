#pragma once

#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QThread>
#include <QtGlobal>
#include <deque>

namespace rnqnx::runtime {

enum class ApplicationVisibility { Unknown, Active, Inactive, Hidden, Suspended };

struct LifecycleSnapshot {
  ApplicationVisibility visibility{ApplicationVisibility::Unknown};
  quint64 sequence{0};
  bool closed{false};
};

// The visibility has two sources. Qt's applicationStateChanged, and the host
// (HostWindowState), which says the window was minimized or restored. The host
// wins while the window is minimized: Hidden (JS: "background"). On restore the
// observer reports Active at once, then follows Qt again. This is how AppState
// changes on QNX Screen, where Qt may report nothing when the native window is
// destroyed (ADR-0024).
//
// Observation only: does not suspend a process, grant background execution,
// persist state, or promise that the platform reports every lifecycle change.
// Construct, read, close and destroy on the QGuiApplication owner thread.
// Connect first, then read snapshot() on that thread for initial state.
class QtApplicationLifecycle final : public QObject {
  Q_OBJECT

 public:
  explicit QtApplicationLifecycle(QGuiApplication& application, QObject* parent = nullptr);
  LifecycleSnapshot snapshot() const;
  void close();
  static ApplicationVisibility visibilityFor(Qt::ApplicationState state) noexcept;

 Q_SIGNALS:
  void stateChanged(rnqnx::runtime::LifecycleSnapshot snapshot);
  void observationClosed();

 private:
  void requireOwner() const;
  void record(Qt::ApplicationState state);
  void recordHostMinimized(bool minimized);
  void push(ApplicationVisibility next);

  QThread* const owner_;
  LifecycleSnapshot snapshot_;
  QMetaObject::Connection stateConnection_;
  QMetaObject::Connection quitConnection_;
  QMetaObject::Connection hostConnection_;
  bool hostMinimized_{false};
  std::deque<ApplicationVisibility> pending_;
  bool draining_{false};
};

}  // namespace rnqnx::runtime

Q_DECLARE_METATYPE(rnqnx::runtime::LifecycleSnapshot)
