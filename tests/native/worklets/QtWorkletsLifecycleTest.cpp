#include "react-native-qnx/worklets/QtFrameBridge.h"
#include "react-native-qnx/worklets/QtUiThread.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QtTest/QtTest>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;
using rnqnx::worklets::QtFrameBridge;
using rnqnx::worklets::QtUiThread;

class QtWorkletsLifecycleTest final : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  void queuedFrameIsDroppedAfterShutdown() {
    QtUiThread ui(QStringLiteral("frame-shutdown-test"));
    QQuickWindow window;
    QtFrameBridge frames(&window, ui);
    std::promise<void> entered;
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic<int> delivered{0};
    QVERIFY(ui.post([&] {
      frames.request([&](double) { ++delivered; });
      entered.set_value();
      gate.wait();
    }));
    const bool enteredInTime = entered.get_future().wait_for(2s) == std::future_status::ready;
    if (!enteredInTime) release.set_value();
    QVERIFY(enteredInTime);
    // Exercise the actual signal/queue route without measuring display timing.
    const bool signalled = QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
    frames.shutdown();
    release.set_value();
    std::promise<void> drained;
    QVERIFY(ui.post([&] { drained.set_value(); }));
    const bool drainedInTime = drained.get_future().wait_for(2s) == std::future_status::ready;
    ui.stop();
    QVERIFY(signalled);
    QVERIFY(drainedInTime);
    QCOMPARE(delivered.load(), 0);
    QCOMPARE(frames.framesDelivered(), std::uint64_t(0));
  }

  void guiPostsAreDiscardedAtShutdown() {
    QtUiThread ui(QStringLiteral("gui-shutdown-test"));
    QQuickWindow window;
    QtFrameBridge frames(&window, ui);
    std::atomic<int> ran{0};
    std::promise<void> posted;
    QVERIFY(ui.post([&] {
      for (int i = 0; i < 1000; ++i) frames.postToGui([&] { ++ran; });
      posted.set_value();
    }));
    const bool postedInTime = posted.get_future().wait_for(2s) == std::future_status::ready;
    frames.shutdown();
    ui.stop();
    QCoreApplication::processEvents();
    QVERIFY(postedInTime);
    QCOMPARE(ran.load(), 0);
    frames.postToGui([&] { ++ran; });
    QCoreApplication::processEvents();
    QCOMPARE(ran.load(), 0);
  }

  void shutdownDropsRemainingCallbacksInRunningBatch() {
    QtUiThread ui(QStringLiteral("running-frame-shutdown-test"));
    QQuickWindow window;
    QtFrameBridge frames(&window, ui);
    std::promise<void> requested;
    std::promise<void> entered;
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic<int> delivered{0};
    QVERIFY(ui.post([&] {
      frames.request([&](double) {
        ++delivered;
        entered.set_value();
        gate.wait();
      });
      frames.request([&](double) { ++delivered; });
      requested.set_value();
    }));
    const bool requestedInTime = requested.get_future().wait_for(2s) == std::future_status::ready;
    const bool signalled = requestedInTime &&
        QMetaObject::invokeMethod(&window, "afterAnimating", Qt::DirectConnection);
    const bool enteredInTime = entered.get_future().wait_for(2s) == std::future_status::ready;
    frames.shutdown();
    release.set_value();
    std::promise<void> drained;
    QVERIFY(ui.post([&] { drained.set_value(); }));
    const bool drainedInTime = drained.get_future().wait_for(2s) == std::future_status::ready;
    ui.stop();
    QVERIFY(requestedInTime);
    QVERIFY(signalled);
    QVERIFY(enteredInTime);
    QVERIFY(drainedInTime);
    QCOMPARE(delivered.load(), 1);
  }

  void concurrentStopsWaitForRunningJobAndDiscardQueuedWork() {
    QtUiThread ui(QStringLiteral("concurrent-stop-test"));
    std::promise<void> entered;
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic<int> queuedRan{0};
    QVERIFY(ui.post([&] {
      entered.set_value();
      gate.wait();
    }));
    const bool enteredInTime = entered.get_future().wait_for(2s) == std::future_status::ready;
    const bool queued = ui.post([&] { ++queuedRan; });
    auto firstStop = std::async(std::launch::async, [&] { ui.stop(); });
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    bool closed = false;
    while (std::chrono::steady_clock::now() < deadline) {
      if (!ui.post([] {})) {
        closed = true;
        break;
      }
      std::this_thread::yield();
    }
    std::promise<void> secondEntered;
    auto secondStop = std::async(std::launch::async, [&] {
      secondEntered.set_value();
      ui.stop();
    });
    const bool secondStarted = secondEntered.get_future().wait_for(2s) == std::future_status::ready;
    const auto secondStatus = secondStop.wait_for(50ms);
    release.set_value();
    firstStop.get();
    secondStop.get();
    QVERIFY(enteredInTime);
    QVERIFY(queued);
    QVERIFY(closed);
    QVERIFY(secondStarted);
    QCOMPARE(secondStatus, std::future_status::timeout);
    QCOMPARE(queuedRan.load(), 0);
    QVERIFY(!ui.post([] {}));
  }

  void stopRacesWithPostsAndRejectsLateWork() {
    for (int trial = 0; trial < 100; ++trial) {
      QtUiThread ui(QStringLiteral("post-stop-test"));
      std::atomic<bool> producerReady{false};
      std::thread producer([&] {
        producerReady.store(true);
        while (ui.post([] {})) {}
      });
      while (!producerReady.load()) std::this_thread::yield();
      ui.stop();
      producer.join();
      QVERIFY(!ui.post([] {}));
      ui.stop();
    }
  }

  void shutdownRacesWithGuiPosts() {
    for (int trial = 0; trial < 100; ++trial) {
      QtUiThread ui(QStringLiteral("gui-post-race-test"));
      QQuickWindow window;
      QtFrameBridge frames(&window, ui);
      std::atomic<bool> producerReady{false};
      std::thread producer([&] {
        producerReady.store(true);
        for (int i = 0; i < 1000; ++i) frames.postToGui([] {});
      });
      while (!producerReady.load()) std::this_thread::yield();
      frames.shutdown();
      producer.join();
      ui.stop();
    }
  }
};

QTEST_MAIN(QtWorkletsLifecycleTest)
#include "QtWorkletsLifecycleTest.moc"
