/*
 * Host tests for the opt-in JavaScript thread (ADR-0021).
 *
 * Each case builds a real Hermes runtime inside a QtReactHost constructed with
 * Options::jsThread. What they prove is host evidence only: Qt offscreen, one
 * macOS or Linux machine. None of it is QNX VM, Pi 400 or release-board
 * evidence, and the flag stays off by default until a device receipt exists.
 *
 * The acceptance case blocks JavaScript for five seconds and checks, while it
 * is blocked, that the title bar controls still act, a pan still moves a
 * Flickable, and the host calls a GUI handler uses do not wait.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#if RNQNX_ENABLE_WINDOW_CHROME
#include "WindowChrome.h"
#include "WindowChromeModel.h"
#endif

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::ShadowViewMutationAdapter;
using rnqnx::runtime::QtReactHost;
using rnqnx::views::QtQuickItemFactory;

constexpr facebook::react::SurfaceId kSurfaceId = 11;
constexpr int kBlockMs = 5000;
// A GUI-side action during the block must finish far inside the block. The
// bound is generous for a loaded CI machine and still far below five seconds.
constexpr int kResponsiveMs = 500;

constexpr const char* kViewSource =
    "import QtQuick\n"
    "Rectangle { id: root; visible: true; property alias backgroundColor: root.color }\n";
constexpr const char* kScrollViewSource =
    "import QtQuick\nFlickable { visible: true; clip: true }\n";

class TestFailure final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(const char* expression, const char* file, int line,
                       const std::string& detail = {}) {
  std::ostringstream message;
  message << file << ':' << line << ": check failed: " << expression;
  if (!detail.empty()) message << " (" << detail << ')';
  throw TestFailure(message.str());
}

#define EXPECT(expression)                       \
  do {                                           \
    if (!(expression)) {                         \
      fail(#expression, __FILE__, __LINE__);     \
    }                                            \
  } while (false)

QtReactHost::Options jsThreadOptions() {
  QtReactHost::Options options;
  options.jsThread = true;
  return options;
}

class Fixture {
 public:
  explicit Fixture(bool jsThread = true)
      : window_(),
#if RNQNX_ENABLE_WINDOW_CHROME
        chrome_(window_, engine_, rnqnx::window::WindowChromeConfig{},
                rnqnx::window::capabilitiesForPlatform(
                    QStringLiteral("cocoa"), {}, rnqnx::window::SurfaceResize::FollowsWindow)),
        factory_(engine_),
        layer_(factory_, chrome_.contentRoot(), kSurfaceId),
#else
        factory_(engine_),
        layer_(factory_, *window_.contentItem(), kSurfaceId),
#endif
        adapter_(layer_),
        host_(adapter_, jsThread ? jsThreadOptions() : QtReactHost::Options{}) {
    window_.resize(640, 480);
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    factory_.registerType(QStringLiteral("ScrollView"), QByteArray(kScrollViewSource));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    adapter_.mapComponent(QStringLiteral("ScrollView"), QStringLiteral("ScrollView"));
    window_.show();
  }
  ~Fixture() { host_.stopSurface(kSurfaceId); }

  QtReactHost& host() { return host_; }
  QQuickWindow& window() { return window_; }
  QtQuickMountingLayer& layer() { return layer_; }
#if RNQNX_ENABLE_WINDOW_CHROME
  rnqnx::window::WindowChrome& chrome() { return chrome_; }
#endif

  void start() {
    host_.installComponentProvider();
    host_.startEmptySurface(kSurfaceId, 640.0F, 480.0F);
  }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
#if RNQNX_ENABLE_WINDOW_CHROME
  rnqnx::window::WindowChrome chrome_;
#endif
  QtQuickItemFactory factory_;
  QtQuickMountingLayer layer_;
  ShadowViewMutationAdapter adapter_;
  QtReactHost host_;
};

/// Reads a global number on the JS thread, the only thread allowed to.
double readNumber(QtReactHost& host, const char* name) {
  double value = -1;
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    const auto property = runtime.global().getProperty(runtime, name);
    value = property.isNumber() ? property.getNumber() : -1;
  });
  return value;
}

void waitUntil(const std::function<bool()>& condition, int timeoutMs, const char* what) {
  QElapsedTimer timer;
  timer.start();
  while (!condition()) {
    if (timer.elapsed() > timeoutMs) {
      fail(what, __FILE__, __LINE__, "timed out");
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QThread::msleep(1);
  }
}

constexpr const char* kScrollTree = R"JS(
  const fabric = globalThis.nativeFabricUIManager;
  const surfaceId = 11;
  const scroll = fabric.createNode(601, 'ScrollView', surfaceId,
    { width: 200, height: 100, collapsable: false }, {});
  const content = fabric.createNode(602, 'View', surfaceId,
    { width: 200, height: 600, collapsable: false }, {});
  fabric.appendChild(scroll, content);
  const set = fabric.createChildSet();
  fabric.appendChildToSet(set, scroll);
  fabric.completeRoot(surfaceId, set);
)JS";

void jsRunsOnItsOwnThread() {
  Fixture fixture;
  auto& host = fixture.host();
  EXPECT(host.jsThreadEnabled());
  EXPECT(!host.onJsThread());
  std::thread::id jsId;
  host.runOnJsThread([&] {
    EXPECT(host.onJsThread());
    jsId = std::this_thread::get_id();
  });
  EXPECT(jsId != std::this_thread::get_id());
  EXPECT(host.evaluate("globalThis.answer = 6 * 7;", "answer.js"));
  EXPECT(readNumber(host, "answer") == 42.0);
  EXPECT(!host.evaluate("throw new Error('boom');", "boom.js"));
  EXPECT(!host.lastError().isEmpty());
  // The flag off keeps JavaScript on the constructing thread.
  Fixture single(false);
  EXPECT(!single.host().jsThreadEnabled());
  EXPECT(single.host().onJsThread());
}

void mountsAreAppliedOnTheGuiThread() {
  Fixture fixture;
  fixture.start();
  EXPECT(fixture.host().hasFabricUIManager());
  EXPECT(fixture.host().evaluate(kScrollTree, "scroll-tree.js"));
  // evaluate() returns after the mount reached the GUI thread.
  QQuickItem* scroll = fixture.layer().itemForTag(601);
  EXPECT(scroll != nullptr);
  EXPECT(scroll->thread() == QThread::currentThread());
  EXPECT(scroll->width() == 200.0);
  QQuickItem* content = fixture.layer().itemForTag(602);
  EXPECT(content != nullptr);
  EXPECT(content->thread() == QThread::currentThread());
}

void timersAndCompletionsRunOnTheJsThread() {
  Fixture fixture;
  auto& host = fixture.host();
  host.installTimers();
  EXPECT(host.evaluate("setTimeout(() => { globalThis.fired = 1; }, 10);", "timer.js"));
  waitUntil([&] { return readNumber(host, "fired") == 1.0; }, 2000, "timer fired");

  // A worker completes through the task queue; the task must run on the JS thread.
  std::atomic<bool> ran{false};
  std::atomic<bool> onJs{false};
  const auto queue = host.taskQueue();
  std::thread worker([&] {
    queue->post([&] {
      onJs = host.onJsThread();
      ran = true;
    });
  });
  worker.join();
  waitUntil([&] { return ran.load(); }, 2000, "queued completion ran");
  EXPECT(onJs.load());
}

void resizeAndWindowSizeDoNotWaitForJavaScript() {
  Fixture fixture;
  fixture.start();
  auto& host = fixture.host();
  std::atomic<bool> done{false};
  host.postToJsThread([&] {
    host.evaluate("const t = Date.now(); while (Date.now() - t < 1500) {}", "spin-short.js");
    done = true;
  });
  QThread::msleep(100);
  EXPECT(!done.load());
  QElapsedTimer timer;
  timer.start();
  host.resizeSurface(kSurfaceId, 800.0F, 600.0F);
  host.setWindowSize(800.0F, 600.0F, 1.0F);
  EXPECT(timer.elapsed() < kResponsiveMs);
  EXPECT(!done.load());
  waitUntil([&] { return done.load(); }, 5000, "short spin ended");
}

void blockedJavaScriptLeavesTheGuiThreadResponsive() {
  Fixture fixture;
  fixture.start();
  auto& host = fixture.host();
  EXPECT(host.evaluate(kScrollTree, "scroll-tree.js"));
  QQuickItem* scroll = fixture.layer().itemForTag(601);
  EXPECT(scroll != nullptr);
  EXPECT(scroll->property("contentHeight").toDouble() > scroll->height());

  std::atomic<bool> blockDone{false};
  std::atomic<int> queuedRan{0};
  QElapsedTimer blocked;
  host.postToJsThread([&] {
    host.evaluate("const t = Date.now(); while (Date.now() - t < 5000) {}", "block.js");
    blockDone = true;
  });
  blocked.start();
  // Work queued during the block must wait for it and then run, in order.
  host.postToJsThread([&] { queuedRan = blockDone.load() ? 1 : -1; });
  QThread::msleep(150);
  EXPECT(!blockDone.load());

  // The GUI event loop keeps running: a 10 ms heartbeat fires throughout.
  int beats = 0;
  QTimer heartbeat;
  heartbeat.setInterval(10);
  heartbeat.setTimerType(Qt::PreciseTimer);
  QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++beats; });
  heartbeat.start();

  const auto elapsedOk = [&](const char* what) {
    if (blockDone.load() || blocked.elapsed() > kBlockMs - 400) {
      fail(what, __FILE__, __LINE__, "the block ended before the check ran");
    }
  };

  // Pan: a real press, drag and release on the Flickable.
  {
    QElapsedTimer action;
    action.start();
    const double before = scroll->property("contentY").toDouble();
    const QPointF start = scroll->mapToScene(QPointF(100, 90));
    const auto send = [&](QEvent::Type type, QPointF point, Qt::MouseButton button,
                          Qt::MouseButtons buttons) {
      QMouseEvent event(type, point, point, button, buttons, Qt::NoModifier);
      QCoreApplication::sendEvent(&fixture.window(), &event);
    };
    send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    const int step = QGuiApplication::styleHints()->startDragDistance() + 6;
    for (int i = 1; i <= 5; ++i) {
      send(QEvent::MouseMove, start - QPointF(0, i * step), Qt::NoButton, Qt::LeftButton);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    const double during = scroll->property("contentY").toDouble();
    send(QEvent::MouseButtonRelease, start - QPointF(0, 5 * step), Qt::LeftButton, Qt::NoButton);
    elapsedOk("pan finished inside the block");
    if (!(during > before + 10.0)) {
      std::ostringstream detail;
      detail << "contentY " << before << " -> " << during;
      fail("pan tracked while JavaScript was blocked", __FILE__, __LINE__, detail.str());
    }
    EXPECT(action.elapsed() < kResponsiveMs);
  }

#if RNQNX_ENABLE_WINDOW_CHROME
  {
    auto& chrome = fixture.chrome();
    QStringList triggered;
    QStringList unavailable;
    QObject::connect(&chrome, &rnqnx::window::WindowChrome::actionTriggered,
                     [&](const QString& action) { triggered << action; });
    QObject::connect(&chrome, &rnqnx::window::WindowChrome::actionUnavailable,
                     [&](const QString& action, const QString&) { unavailable << action; });
    QElapsedTimer action;
    action.start();
    chrome.trigger(QStringLiteral("minimize"));
    EXPECT(action.elapsed() < kResponsiveMs);
    elapsedOk("minimize finished inside the block");
    EXPECT(triggered == QStringList{QStringLiteral("minimize")});
    EXPECT(unavailable.isEmpty());
    EXPECT(fixture.window().visibility() == QWindow::Minimized);
    fixture.window().showNormal();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    EXPECT(fixture.window().isVisible());

    action.restart();
    chrome.trigger(QStringLiteral("close"));
    EXPECT(action.elapsed() < kResponsiveMs);
    elapsedOk("close finished inside the block");
    EXPECT((triggered == QStringList{QStringLiteral("minimize"), QStringLiteral("close")}));
    EXPECT(!fixture.window().isVisible());
  }
#endif

  // Posted GUI-handler calls return at once even though the runtime is busy.
  {
    QElapsedTimer action;
    action.start();
    host.resizeSurface(kSurfaceId, 700.0F, 500.0F);
    host.setWindowSize(700.0F, 500.0F, 1.0F);
    EXPECT(action.elapsed() < kResponsiveMs);
    elapsedOk("resize calls returned inside the block");
  }

  // The loop ran the whole time: at 10 ms, 150 ms of block alone allows 10.
  const qint64 sampled = blocked.elapsed();
  waitUntil([&] { return blockDone.load(); }, kBlockMs + 3000, "the block ended");
  EXPECT(blocked.elapsed() >= kBlockMs - 100);
  EXPECT(beats >= sampled / 40);
  waitUntil([&] { return queuedRan.load() != 0; }, 2000, "queued work ran after the block");
  EXPECT(queuedRan.load() == 1);
  std::cout << "blocked=" << blocked.elapsed() << "ms beats=" << beats << std::endl;
}

void teardownWithQueuedWorkIsClean() {
  for (int round = 0; round < 3; ++round) {
    Fixture fixture;
    fixture.start();
    auto& host = fixture.host();
    host.installTimers();
    EXPECT(host.evaluate(kScrollTree, "scroll-tree.js"));
    EXPECT(host.evaluate("setInterval(() => {}, 1);", "interval.js"));
    const auto queue = host.taskQueue();
    std::atomic<bool> stop{false};
    std::thread producer([&] {
      while (!stop.load()) {
        try {
          queue->post([] {});
        } catch (const std::exception&) {
          return;  // closed during teardown; the contract is a throw, not a crash
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    });
    QThread::msleep(30);
    stop = true;
    producer.join();
  }
}

struct TestCase {
  const char* name;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  const std::vector<TestCase> tests{
      {"jsRunsOnItsOwnThread", jsRunsOnItsOwnThread},
      {"mountsAreAppliedOnTheGuiThread", mountsAreAppliedOnTheGuiThread},
      {"timersAndCompletionsRunOnTheJsThread", timersAndCompletionsRunOnTheJsThread},
      {"resizeAndWindowSizeDoNotWaitForJavaScript", resizeAndWindowSizeDoNotWaitForJavaScript},
      {"blockedJavaScriptLeavesTheGuiThreadResponsive",
       blockedJavaScriptLeavesTheGuiThreadResponsive},
      {"teardownWithQueuedWorkIsClean", teardownWithQueuedWorkIsClean},
  };
  int failed = 0;
  for (const auto& test : tests) {
    try {
      test.run();
      std::cout << "[PASS] " << test.name << std::endl;
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
    }
  }
#if !RNQNX_ENABLE_WINDOW_CHROME
  std::cout << "[NOTE] title bar controls not exercised: RNQNX_ENABLE_WINDOW_CHROME=OFF" << std::endl;
#endif
  if (failed != 0) {
    std::cerr << failed << " JS-thread host test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All " << tests.size() << " JS-thread host tests passed.\n";
  return EXIT_SUCCESS;
}
