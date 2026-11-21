#include <rnqnx/testing/NodeQuery.h>

#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

#include <chrono>

using namespace std::chrono_literals;
namespace t = rnqnx::testing;

class NodeWaitSmokeTest final : public QObject {
  Q_OBJECT
 private Q_SLOTS:
  // Host lane, offscreen platform. RNQNX_FIXTURE_NODE_NAME renames the node the
  // fixture creates; the query keeps asking for "fixtureReady", so the mutated
  // run must fail.
  void waitsForDeferredNode() {
    QQuickView view;
    view.setSource(QUrl::fromLocalFile(QStringLiteral(RNQNX_FIXTURE_QML)));
    QVERIFY2(view.status() == QQuickView::Ready, "fixture QML did not load");
    const QByteArray renamed = qgetenv("RNQNX_FIXTURE_NODE_NAME");
    if (!renamed.isEmpty()) view.rootObject()->setProperty("nodeName", QString::fromUtf8(renamed));
    // The fixture creates its node 150 ms after the scene starts running; the
    // first scan must therefore miss it and the wait must do the work.
    view.show();

    t::NodeQuery query;
    query.objectName = QStringLiteral("fixtureReady");
    query.requireVisible = true;
    const t::NodeWaitResult result = t::waitForNode(view.rootObject(), query, 3s);
    QVERIFY2(result.found(), qPrintable(result.error));
    QCOMPARE(result.node->objectName(), QStringLiteral("fixtureReady"));
    QVERIFY2(result.elapsed > 0ms, "the node existed before the wait: it did not test the wait");
  }
};

QTEST_MAIN(NodeWaitSmokeTest)
#include "NodeWaitSmokeTest.moc"
