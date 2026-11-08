#include "QtApplicationLifecycle.h"

#include <QPointer>
#include <QSignalSpy>
#include <QtTest>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

using rnqnx::runtime::ApplicationVisibility;
using rnqnx::runtime::LifecycleSnapshot;
using rnqnx::runtime::QtApplicationLifecycle;

// The real Qt signal path is exercised with synthetic lifecycle inputs.
// This does not ask a window manager to hide or suspend the process.
static void notify(Qt::ApplicationState state) {
  const bool invoked = QMetaObject::invokeMethod(qGuiApp, "applicationStateChanged",
      Qt::DirectConnection, Q_ARG(Qt::ApplicationState, state));
  if (!invoked) throw std::runtime_error("Qt lifecycle signal was not found.");
}

class LifecycleTest final : public QObject {
  Q_OBJECT
 private slots:
  void initialSnapshot() {
    QtApplicationLifecycle observer(*qGuiApp);
    const auto initial = observer.snapshot();
    QCOMPARE(initial.visibility, QtApplicationLifecycle::visibilityFor(qGuiApp->applicationState()));
    QCOMPARE(initial.sequence, quint64{0});
    QVERIFY(!initial.closed);
  }
  void statesAndDuplicateSuppression() {
    QtApplicationLifecycle observer(*qGuiApp);
    QSignalSpy changes(&observer, &QtApplicationLifecycle::stateChanged);
    const std::vector<Qt::ApplicationState> states{Qt::ApplicationActive,
        Qt::ApplicationInactive, Qt::ApplicationHidden, Qt::ApplicationSuspended};
    for (const auto state : states) {
      notify(state);
      QCOMPARE(observer.snapshot().visibility, QtApplicationLifecycle::visibilityFor(state));
      const auto before = observer.snapshot().sequence;
      const auto count = changes.count();
      notify(state);
      QCOMPARE(observer.snapshot().sequence, before);
      QCOMPARE(changes.count(), count);
    }
    notify(static_cast<Qt::ApplicationState>(0x7fff));
    QCOMPARE(observer.snapshot().visibility, ApplicationVisibility::Unknown);
  }
  void independentObserversAndClose() {
    QtApplicationLifecycle first(*qGuiApp), second(*qGuiApp);
    QSignalSpy closed(&first, &QtApplicationLifecycle::observationClosed);
    notify(Qt::ApplicationActive);
    const auto before = first.snapshot();
    first.close();
    first.close();
    notify(Qt::ApplicationHidden);
    QCOMPARE(closed.count(), 1);
    QVERIFY(first.snapshot().closed);
    QCOMPARE(first.snapshot().sequence, before.sequence);
    QCOMPARE(first.snapshot().visibility, before.visibility);
    QCOMPARE(second.snapshot().visibility, ApplicationVisibility::Hidden);
  }
  void reentrantDeliveryIsOrdered() {
    QtApplicationLifecycle observer(*qGuiApp);
    notify(Qt::ApplicationInactive);
    std::vector<quint64> observed;
    connect(&observer, &QtApplicationLifecycle::stateChanged, &observer,
        [](LifecycleSnapshot state) {
          if (state.visibility == ApplicationVisibility::Active) notify(Qt::ApplicationHidden);
        });
    connect(&observer, &QtApplicationLifecycle::stateChanged, &observer,
        [&](LifecycleSnapshot state) { observed.push_back(state.sequence); });
    notify(Qt::ApplicationActive);
    QCOMPARE(observed.size(), std::size_t{2});
    QVERIFY(observed[0] < observed[1]);
    QCOMPARE(observer.snapshot().visibility, ApplicationVisibility::Hidden);
  }
  void closeDuringDeliveryDropsPending() {
    QtApplicationLifecycle observer(*qGuiApp);
    notify(Qt::ApplicationInactive);
    QSignalSpy changes(&observer, &QtApplicationLifecycle::stateChanged);
    connect(&observer, &QtApplicationLifecycle::stateChanged, &observer,
        [&](LifecycleSnapshot) { notify(Qt::ApplicationHidden); observer.close(); });
    notify(Qt::ApplicationActive);
    QCOMPARE(changes.count(), 1);
    QVERIFY(observer.snapshot().closed);
  }
  void deleteDuringDelivery() {
    auto* observer = new QtApplicationLifecycle(*qGuiApp);
    notify(Qt::ApplicationInactive);
    QPointer<QtApplicationLifecycle> alive(observer);
    connect(observer, &QtApplicationLifecycle::stateChanged, qGuiApp,
        [observer](LifecycleSnapshot) { delete observer; });
    notify(Qt::ApplicationActive);
    QVERIFY(alive.isNull());
    notify(Qt::ApplicationHidden);
  }
  void foreignThreadReadRejected() {
    QtApplicationLifecycle observer(*qGuiApp);
    std::atomic<bool> rejected{false};
    std::thread worker([&] {
      try { (void)observer.snapshot(); }
      catch (const std::logic_error&) { rejected = true; }
    });
    worker.join();
    QVERIFY(rejected.load());
  }
};

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  LifecycleTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "LifecycleTest.moc"
