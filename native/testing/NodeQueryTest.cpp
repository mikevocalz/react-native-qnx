#include <rnqnx/testing/NodeQuery.h>

#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
namespace t = rnqnx::testing;

namespace {

t::NodeQuery named(const char* name, bool visible = false) {
  t::NodeQuery query;
  query.objectName = QString::fromLatin1(name);
  query.requireVisible = visible;
  return query;
}

}  // namespace

class NodeQueryTest final : public QObject {
  Q_OBJECT
 private Q_SLOTS:
  void findsQObjectChildAndVisualChild() {
    QQuickItem root;
    auto* child = new QObject(&root);
    child->setObjectName(QStringLiteral("objectChild"));
    // A visual child whose QObject parent is a different object: only
    // childItems() reaches it.
    QObject owner;
    auto* visual = new QQuickItem;
    visual->setObjectName(QStringLiteral("visualChild"));
    visual->setParentItem(&root);
    QCOMPARE(t::findNode(&root, named("objectChild")), child);
    QCOMPARE(t::findNode(&root, named("visualChild")), visual);
    QVERIFY(t::findNode(&root, named("absent")) == nullptr);
    delete visual;
  }

  void rootCanMatch() {
    QObject root;
    root.setObjectName(QStringLiteral("self"));
    QCOMPARE(t::findNode(&root, named("self")), &root);
  }

  void firstVisibleMatchWins() {
    QQuickItem root;
    QQuickItem hidden(&root);
    hidden.setObjectName(QStringLiteral("dup"));
    hidden.setVisible(false);
    QQuickItem shown(&root);
    shown.setObjectName(QStringLiteral("dup"));
    QCOMPARE(t::findNode(&root, named("dup", true)), &shown);
  }

  void rejectsBadArguments() {
    QObject root;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, t::findNode(nullptr, named("x")));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, t::findNode(&root, named("")));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, t::waitForNode(&root, named("x"), 0ms));
    root.setObjectName(QStringLiteral("x"));
    QVERIFY_THROWS_EXCEPTION(std::logic_error, t::findNode(&root, named("x", true)));
  }

  void rejectsRootFromAnotherThread() {
    auto* foreign = new QObject;
    QThread thread;
    foreign->moveToThread(&thread);
    QVERIFY_THROWS_EXCEPTION(std::logic_error, t::findNode(foreign, named("x")));
    QVERIFY_THROWS_EXCEPTION(std::logic_error, t::waitForNode(foreign, named("x"), 10ms));
    thread.start();
    QMetaObject::invokeMethod(foreign, &QObject::deleteLater);
    thread.quit();
    QVERIFY(thread.wait(5000));
  }

  // The timeout path: a node that never appears fails, names itself, and does
  // not return before the budget.
  void timesOutNamingTheNode() {
    QQuickItem root;
    root.setObjectName(QStringLiteral("main"));
    const t::NodeWaitResult result = t::waitForNode(&root, named("noSuchNode"), 200ms);
    QCOMPARE(result.status, t::NodeWaitStatus::TimedOut);
    QVERIFY(!result.found());
    QVERIFY2(result.elapsed >= 200ms, qPrintable(QString::number(result.elapsed.count())));
    QVERIFY2(result.elapsed < 2000ms, qPrintable(QString::number(result.elapsed.count())));
    QVERIFY2(result.error.contains(QStringLiteral("'noSuchNode'")), qPrintable(result.error));
    QVERIFY2(result.error.contains(QStringLiteral("200 ms")), qPrintable(result.error));
    QVERIFY2(result.error.contains(QStringLiteral("QQuickItem 'main'")), qPrintable(result.error));
  }

  // Event-driven: the node is created by a queued event, not on a timer the
  // wait knows about, and the wait returns well before its budget.
  void findsNodeCreatedByAQueuedEvent() {
    QQuickItem root;
    QMetaObject::invokeMethod(
        &root,
        [&root] {
          auto* late = new QQuickItem(&root);
          late->setObjectName(QStringLiteral("late"));
        },
        Qt::QueuedConnection);
    const t::NodeWaitResult result = t::waitForNode(&root, named("late"), 5s);
    QVERIFY2(result.found(), qPrintable(result.error));
    QVERIFY(result.elapsed < 1000ms);
  }

  void findsNodeRenamedLaterByAnEvent() {
    QQuickItem root;
    QQuickItem item(&root);
    QTimer::singleShot(40, &item, [&item] { item.setObjectName(QStringLiteral("renamedLater")); });
    const t::NodeWaitResult result = t::waitForNode(&root, named("renamedLater"), 5s);
    QVERIFY2(result.found(), qPrintable(result.error));
    QCOMPARE(result.node.data(), &item);
  }

  void reportsNotVisibleAtTheDeadline() {
    QQuickItem root;
    QQuickItem item(&root);
    item.setObjectName(QStringLiteral("hiddenOne"));
    item.setVisible(false);
    const t::NodeWaitResult result = t::waitForNode(&root, named("hiddenOne", true), 150ms);
    QCOMPARE(result.status, t::NodeWaitStatus::NotVisible);
    QVERIFY2(result.error.contains(QStringLiteral("'hiddenOne'")), qPrintable(result.error));
  }

  void waitsUntilTheNodeBecomesVisible() {
    QQuickItem root;
    QQuickItem item(&root);
    item.setObjectName(QStringLiteral("soonVisible"));
    item.setVisible(false);
    QTimer::singleShot(40, &item, [&item] { item.setVisible(true); });
    const t::NodeWaitResult result = t::waitForNode(&root, named("soonVisible", true), 5s);
    QVERIFY2(result.found(), qPrintable(result.error));
  }

  void reportsRootDestroyed() {
    auto* root = new QQuickItem;
    QTimer::singleShot(30, root, [root] { delete root; });
    const t::NodeWaitResult result = t::waitForNode(root, named("never"), 5s);
    QCOMPARE(result.status, t::NodeWaitStatus::RootDestroyed);
    QVERIFY(result.elapsed < 2000ms);
  }

  void refusesANestedWait() {
    QQuickItem root;
    bool threw = false;
    QTimer::singleShot(20, &root, [&] {
      try {
        t::waitForNode(&root, named("inner"), 50ms);
      } catch (const std::logic_error&) {
        threw = true;
      }
    });
    const t::NodeWaitResult result = t::waitForNode(&root, named("outer"), 300ms);
    QCOMPARE(result.status, t::NodeWaitStatus::TimedOut);
    QVERIFY(threw);
    // The refusal must not leave the thread marked busy.
    QCOMPARE(t::waitForNode(&root, named("again"), 20ms).status, t::NodeWaitStatus::TimedOut);
  }

  // quit() does nothing unless an application loop is running, and QtTest does
  // not run one, so this case starts one. Keep it last: it ends the loop.
  void reportsAppQuit() {
    QQuickItem root;
    t::NodeWaitResult result;
    QTimer::singleShot(0, &root, [&] {
      QTimer::singleShot(20, &root, [] { QCoreApplication::quit(); });
      result = t::waitForNode(&root, named("never"), 5s);
    });
    QCoreApplication::exec();
    QCOMPARE(result.status, t::NodeWaitStatus::AppQuit);
    QVERIFY(result.elapsed < 2000ms);
  }
};

QTEST_MAIN(NodeQueryTest)
#include "NodeQueryTest.moc"
