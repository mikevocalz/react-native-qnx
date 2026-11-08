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

  QThread* const owner_;
  LifecycleSnapshot snapshot_;
  QMetaObject::Connection stateConnection_;
  QMetaObject::Connection quitConnection_;
  std::deque<ApplicationVisibility> pending_;
  bool draining_{false};
};

}  // namespace rnqnx::runtime

Q_DECLARE_METATYPE(rnqnx::runtime::LifecycleSnapshot)
