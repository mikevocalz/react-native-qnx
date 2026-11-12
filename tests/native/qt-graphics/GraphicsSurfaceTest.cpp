#include "QtGraphicsSurface.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QSGRendererInterface>
#include <QTest>
#include <QTimer>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace rnqnx::graphics;

namespace {

// The deadline only bounds a failure. Progress is driven by real Qt signals,
// without polling sleeps or synthetic sceneGraph/frameSwapped emissions.
bool waitForObservation(QtGraphicsSurface& observer, const std::function<bool()>& ready) {
  if (ready()) return true;
  QEventLoop loop;
  QTimer deadline;
  deadline.setSingleShot(true);
  QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
  const auto inspect = [&] { if (ready()) loop.quit(); };
  QObject::connect(&observer, &QtGraphicsSurface::snapshotChanged, &loop, inspect);
  QObject::connect(&observer, &QtGraphicsSurface::frameSwapped, &loop, inspect);
  deadline.start(2500);
  loop.exec();
  return ready();
}

}  // namespace

class GraphicsSurfaceTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void viewportValidation() {
    const auto viewport = GraphicsViewport::fromLogicalSize({301, 211}, 1.25);
    QCOMPARE(viewport.logicalSize, QSize(301, 211));
    QCOMPARE(viewport.pixelSize, QSize(376, 264));
    QCOMPARE(GraphicsViewport::fromLogicalSize({0, 0}, 2).pixelSize, QSize(0, 0));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, GraphicsViewport::fromLogicalSize({1, 1}, 0));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, GraphicsViewport::fromLogicalSize({1, 1}, -1));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, GraphicsViewport::fromLogicalSize({1, 1}, std::numeric_limits<qreal>::infinity()));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, GraphicsViewport::fromLogicalSize({1, 1}, std::numeric_limits<qreal>::quiet_NaN()));
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, GraphicsViewport::fromLogicalSize({-1, 1}, 1));
    QVERIFY_THROWS_EXCEPTION(std::overflow_error, GraphicsViewport::fromLogicalSize({std::numeric_limits<int>::max(), 1}, 2));
  }

  void realWindowLifecycle() {
    QQuickWindow window;
    window.resize(301, 211);
    window.setPersistentGraphics(false);
    window.setPersistentSceneGraph(false);
    const bool initiallyInitialized = window.isSceneGraphInitialized();
    const auto expectedBackend = qEnvironmentVariable("QSG_RHI_BACKEND") == "null"
        ? GraphicsBackend::Null : GraphicsBackend::Software;
    QtGraphicsSurface observer(window);
    QSignalSpy snapshots(&observer, &QtGraphicsSurface::snapshotChanged);
    QSignalSpy frames(&observer, &QtGraphicsSurface::frameSwapped);
    bool callbacksOnOwner = true;
    connect(&observer, &QtGraphicsSurface::snapshotChanged, &observer, [&] {
      callbacksOnOwner = callbacksOnOwner && QThread::currentThread() == qApp->thread();
    });
    connect(&observer, &QtGraphicsSurface::frameSwapped, &observer, [&] {
      callbacksOnOwner = callbacksOnOwner && QThread::currentThread() == qApp->thread();
    });

    const auto initial = observer.snapshot();
    QCOMPARE(initial.schemaVersion, quint32(1));
    QCOMPARE(initial.state, initiallyInitialized ? GraphicsSurfaceState::Initialized : GraphicsSurfaceState::Uninitialized);
    QCOMPARE(initial.backend, initiallyInitialized ? expectedBackend : GraphicsBackend::Unknown);
    QVERIFY(!initial.isVisible);
    QVERIFY(!initial.isExposed);
    QCOMPARE(initial.viewport.logicalSize, QSize(301, 211));
    QCOMPARE(initial.viewport.devicePixelRatio, qreal(1.25));
    QCOMPARE(initial.viewport.pixelSize, QSize(376, 264));

    window.show();
    QVERIFY(waitForObservation(observer, [&] {
      return observer.snapshot().state == GraphicsSurfaceState::Initialized && !frames.isEmpty();
    }));
    const auto active = observer.snapshot();
    QCOMPARE(active.backend, expectedBackend);
    QVERIFY(active.isVisible);
    QVERIFY(active.isExposed);
    QVERIFY(active.generation > initial.generation);
    const auto firstFrame = qvariant_cast<GraphicsFrameSwap>(frames.front().front());
    QCOMPARE(firstFrame.sequence, quint64(1));
    QVERIFY(firstFrame.observedAtNanoseconds >= 0);

    const auto previousFrameCount = frames.size();
    window.resize(401, 231);
    QVERIFY(waitForObservation(observer, [&] {
      return observer.snapshot().viewport.pixelSize == QSize(501, 289);
    }));
    QVERIFY(observer.snapshot().generation > active.generation);
    window.update();
    QVERIFY(waitForObservation(observer, [&] { return frames.size() > previousFrameCount; }));
    const auto secondFrame = qvariant_cast<GraphicsFrameSwap>(frames.back().front());
    QVERIFY(secondFrame.sequence > firstFrame.sequence);
    QVERIFY(secondFrame.observedAtNanoseconds >= firstFrame.observedAtNanoseconds);

    const auto beforeInvalidation = observer.snapshot().generation;
    window.hide();
    QVERIFY(!observer.snapshot().isVisible);
    window.releaseResources();
    // The software/basic render loop may retain its shared scene graph even
    // after releaseResources(). Destroy the real native surface explicitly;
    // this exercises the public PlatformSurface invalidation/recreation path.
    window.destroy();
    QVERIFY(waitForObservation(observer, [&] {
      return observer.snapshot().state == GraphicsSurfaceState::Invalidated;
    }));
    QCOMPARE(observer.snapshot().backend, GraphicsBackend::Unknown);
    QVERIFY(observer.snapshot().generation > beforeInvalidation);
    const auto beforeRecreation = observer.snapshot().generation;
    const auto beforeRecreationFrames = frames.size();
    window.show();
    QVERIFY(waitForObservation(observer, [&] {
      return observer.snapshot().state == GraphicsSurfaceState::Initialized && frames.size() > beforeRecreationFrames;
    }));
    QVERIFY(observer.snapshot().generation > beforeRecreation);
    QCOMPARE(observer.snapshot().backend, expectedBackend);
    QVERIFY(callbacksOnOwner);

    quint64 sequence = 0;
    for (const auto& event : snapshots) {
      const auto snapshot = qvariant_cast<GraphicsSurfaceSnapshot>(event.front());
      QVERIFY(snapshot.sequence > sequence);
      sequence = snapshot.sequence;
    }
  }

  void destroyWindowDropsPendingEvents() {
    auto window = std::make_unique<QQuickWindow>();
    window->resize(80, 60);
    QtGraphicsSurface observer(*window);
    QSignalSpy frames(&observer, &QtGraphicsSurface::frameSwapped);
    window->show();
    QVERIFY(waitForObservation(observer, [&] { return !frames.isEmpty(); }));
    const auto previous = observer.snapshot().generation;
    window->update();
    window.reset();
    QCOMPARE(observer.snapshot().state, GraphicsSurfaceState::Destroyed);
    QCOMPARE(observer.snapshot().backend, GraphicsBackend::Unknown);
    QVERIFY(observer.snapshot().generation > previous);
    const auto count = frames.size();
    QCoreApplication::sendPostedEvents();
    QCOMPARE(frames.size(), count);
    observer.close();
    QCOMPARE(observer.snapshot().state, GraphicsSurfaceState::Closed);
    const auto closedSequence = observer.snapshot().sequence;
    observer.close();
    QCOMPARE(observer.snapshot().sequence, closedSequence);
  }

  void closeDuringDeliveryAndWrongThread() {
    QQuickWindow window;
    QtGraphicsSurface observer(window);
    std::vector<GraphicsSurfaceSnapshot> delivered;
    connect(&observer, &QtGraphicsSurface::snapshotChanged, &observer,
        [&](GraphicsSurfaceSnapshot state) {
          if (state.state != GraphicsSurfaceState::Closed) observer.close();
        });
    connect(&observer, &QtGraphicsSurface::snapshotChanged, &observer,
        [&](GraphicsSurfaceSnapshot state) { delivered.push_back(state); });
    window.resize(40, 30);
    QCOMPARE(observer.snapshot().state, GraphicsSurfaceState::Closed);
    QCOMPARE(delivered.size(), size_t(2));
    QVERIFY(delivered[1].sequence > delivered[0].sequence);
    QCOMPARE(delivered[1].state, GraphicsSurfaceState::Closed);

    bool rejected = false;
    std::thread worker([&] {
      try { (void)observer.snapshot(); }
      catch (const std::logic_error&) { rejected = true; }
    });
    worker.join();
    QVERIFY(rejected);
  }

  void deleteDuringDelivery() {
    QQuickWindow window;
    auto* observer = new QtGraphicsSurface(window);
    QPointer<QtGraphicsSurface> alive(observer);
    connect(observer, &QtGraphicsSurface::snapshotChanged, &window, [observer] { delete observer; });
    window.resize(20, 20);
    QVERIFY(!alive);
    window.resize(30, 30);
    QCoreApplication::sendPostedEvents();
  }
};

int main(int argc, char** argv) {
  if (qEnvironmentVariable("QSG_RHI_BACKEND") == "null") {
    QQuickWindow::setSceneGraphBackend("rhi");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Null);
  }
  QGuiApplication application(argc, argv);
  GraphicsSurfaceTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "GraphicsSurfaceTest.moc"
