/*
 * Tests for the opt-in JavaScript thread (ADR-0021).
 *
 * Each case builds a real Hermes runtime inside a QtReactHost constructed with
 * Options::jsThread. Which target a run proves depends on where it ran. The
 * CTest entry is Qt offscreen on one macOS or Linux machine. The same source is
 * also cross-built for QNX (RNQNX_BUILD_JS_THREAD_QNX_TEST) and run on the QEMU
 * VM, which is QNX VM evidence (TCG, software render) and never Pi 400 or
 * release-board evidence. The flag stays off by default until a device receipt
 * exists.
 *
 * Scripts live in tests/fixtures/js-thread. The host evaluates the .js source.
 * The QNX Hermes is the lean build, which only runs bytecode, so the QNX build
 * compiles the same files with hermesc and RNQNX_JS_THREAD_SCRIPTS points at
 * the .hbc directory.
 *
 * The acceptance case blocks JavaScript (five seconds by default) and checks,
 * while it is blocked, that the title bar controls still act, a pan still moves
 * a Flickable, touch events queue for JavaScript, and the host calls a GUI
 * handler uses do not wait. A control case runs the same block with the flag
 * off and checks that none of that works.
 *
 * Knobs for a slow target, all optional, all printed with the results:
 *   RNQNX_JS_THREAD_SCRIPTS        directory of fixture scripts
 *   RNQNX_JS_THREAD_BLOCK_MS       how long JavaScript is blocked (5000)
 *   RNQNX_JS_THREAD_RESPONSIVE_MS  bound for a GUI action during the block (500)
 *   RNQNX_JS_THREAD_SHOTS          directory for mid-block window grabs
 * Arguments: --only <case> (repeatable) runs just those cases.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#if RNQNX_ENABLE_WINDOW_CHROME
#include "WindowChrome.h"
#include "WindowChromeModel.h"
#endif

#if defined(RNQNX_JS_THREAD_TEST_WORKLETS)
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"
#endif
#if defined(RNQNX_JS_THREAD_TEST_REANIMATED)
#include "react-native-qnx/reanimated/QtReanimatedPlatform.h"
#endif
#if defined(RNQNX_JS_THREAD_TEST_GESTURE)
#include "react-native-qnx/gesture/QtGesturePlatform.h"
#endif
#if defined(RNQNX_JS_THREAD_TEST_NITRO)
#include "QnxNitroRuntime.hpp"
#include "QtNitroDispatcher.hpp"

#include <NitroModules/JSIConverter+Callback.hpp>
#include <NitroModules/JSIConverter+Promise.hpp>
#endif

#include <jsi/jsi.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QStyleHints>
#include <QThread>
#include <QTimer>
#include <QUrl>
#if defined(RNQNX_JS_THREAD_TEST_WEBSOCKET)
#include <QWebSocket>
#include <QWebSocketServer>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::QtTouchDispatcher;
using rnqnx::fabric::ShadowViewMutationAdapter;
using rnqnx::runtime::QtReactHost;
using rnqnx::views::QtQuickItemFactory;

constexpr facebook::react::SurfaceId kSurfaceId = 11;

int environmentInt(const char* name, int fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') return fallback;
  const int parsed = std::atoi(value);
  return parsed > 0 ? parsed : fallback;
}

// How long JavaScript is blocked. Five seconds is the acceptance figure.
const int kBlockMs = environmentInt("RNQNX_JS_THREAD_BLOCK_MS", 5000);
// A GUI-side action during the block must finish far inside the block. The
// bound is generous for a loaded CI machine and still far below five seconds.
const int kResponsiveMs = environmentInt("RNQNX_JS_THREAD_RESPONSIVE_MS", 500);

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

std::string readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) throw TestFailure("cannot open script " + path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

/// The text or bytecode of a fixture script, see the header comment.
std::string scriptSource(const char* name) {
  const char* directory = std::getenv("RNQNX_JS_THREAD_SCRIPTS");
  return readFile(std::string(directory != nullptr ? directory : RNQNX_JS_THREAD_SCRIPT_DIR) + "/" +
                  name + RNQNX_JS_THREAD_SCRIPT_EXT);
}

bool runScript(QtReactHost& host, const char* name) {
  return host.evaluate(scriptSource(name), std::string(name) + ".js");
}

void mustRun(QtReactHost& host, const char* name) {
  if (!runScript(host, name)) {
    fail(name, __FILE__, __LINE__, host.lastError().toStdString());
  }
}

/// The capability set the chrome is built for. The offscreen platform has no
/// window manager, so the host run asks for the desktop set, as the title bar
/// does under a real one; QNX gets its own.
QString chromePlatform() {
  return QGuiApplication::platformName() == QLatin1String("qnx") ? QStringLiteral("qnx")
                                                                 : QStringLiteral("cocoa");
}

class Fixture {
 public:
  explicit Fixture(bool jsThread = true)
      : window_(),
#if RNQNX_ENABLE_WINDOW_CHROME
        chrome_(window_, engine_, rnqnx::window::WindowChromeConfig{},
                rnqnx::window::capabilitiesForPlatform(
                    chromePlatform(), {}, rnqnx::window::SurfaceResize::FollowsWindow)),
        factory_(engine_),
        layer_(factory_, chrome_.contentRoot(), kSurfaceId),
#else
        factory_(engine_),
        layer_(factory_, *window_.contentItem(), kSurfaceId),
#endif
        adapter_(layer_),
#if RNQNX_ENABLE_WINDOW_CHROME
        dispatcher_(layer_, chrome_.contentRoot()),
#else
        dispatcher_(layer_, *window_.contentItem()),
#endif
        host_(adapter_, jsThread ? jsThreadOptions() : QtReactHost::Options{}) {
    window_.resize(640, 480);
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    factory_.registerType(QStringLiteral("ScrollView"), QByteArray(kScrollViewSource));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    adapter_.mapComponent(QStringLiteral("ScrollView"), QStringLiteral("ScrollView"));
    // The input route the application uses: the dispatcher filters the
    // window's pointer events and hands them to Fabric's event emitters.
    adapter_.setTouchDispatcher(&dispatcher_);
    host_.delegate().setTouchDispatcher(&dispatcher_);
    window_.installEventFilter(&dispatcher_);
    window_.show();
  }
  ~Fixture() { host_.stopSurface(kSurfaceId); }

  QtReactHost& host() { return host_; }
  QQuickWindow& window() { return window_; }
  QtQuickMountingLayer& layer() { return layer_; }
  ShadowViewMutationAdapter& adapter() { return adapter_; }
  QtTouchDispatcher& dispatcher() { return dispatcher_; }
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
  // Declared before the host: stopping the surface commits an empty tree
  // during host destruction and that reaches the adapter and the dispatcher.
  QtTouchDispatcher dispatcher_;
  QtReactHost host_;
};

#if RNQNX_ENABLE_WINDOW_CHROME
QQuickItem* findItemNamed(QQuickItem* root, const QString& name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem* child : root->childItems()) {
    if (auto* found = findItemNamed(child, name)) return found;
  }
  return nullptr;
}
#endif

bool installDimensionsRecorder(QtReactHost& host) {
  return runScript(host, "dims-recorder");
}

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

std::string readString(QtReactHost& host, const char* name) {
  std::string value;
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    const auto property = runtime.global().getProperty(runtime, name);
    value = property.isString() ? property.getString(runtime).utf8(runtime) : "<not a string>";
  });
  return value;
}

/// Reads `globalThis.<object>.<key>` as a string, for the module lookup table.
std::string readEntry(QtReactHost& host, const char* object, const char* key) {
  std::string value;
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    const auto table = runtime.global().getProperty(runtime, object);
    if (!table.isObject()) {
      value = "<no table>";
      return;
    }
    const auto entry = table.getObject(runtime).getProperty(runtime, key);
    value = entry.isString() ? entry.getString(runtime).utf8(runtime) : "<not a string>";
  });
  return value;
}

void setNumber(QtReactHost& host, const char* name, double value) {
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    runtime.global().setProperty(runtime, name, value);
  });
}

void setString(QtReactHost& host, const char* name, const std::string& value) {
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    runtime.global().setProperty(
        runtime, name, facebook::jsi::String::createFromUtf8(runtime, value));
  });
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

void result(const std::string& line) { std::cout << "[RESULT] " << line << std::endl; }

/// Writes a grab of the window when RNQNX_JS_THREAD_SHOTS names a directory.
void grab(QQuickWindow& window, const char* name) {
  const char* directory = std::getenv("RNQNX_JS_THREAD_SHOTS");
  if (directory == nullptr || *directory == '\0') return;
  QDir().mkpath(QString::fromUtf8(directory));
  const QImage image = window.grabWindow();
  const QString path =
      QDir(QString::fromUtf8(directory)).filePath(QString::fromUtf8(name) + QStringLiteral(".png"));
  if (image.isNull() || !image.save(path)) {
    result(std::string("grab ") + name + " failed (null image or write error)");
    return;
  }
  result(std::string("grab ") + name + " " + std::to_string(image.width()) + "x" +
         std::to_string(image.height()) + " " + path.toStdString());
}

/// Sends a mouse event straight to the window, as the platform plugin would.
void sendMouse(QQuickWindow& window, QEvent::Type type, QPointF point, Qt::MouseButton button,
               Qt::MouseButtons buttons) {
  QMouseEvent event(type, point, point, button, buttons, Qt::NoModifier);
  QCoreApplication::sendEvent(&window, &event);
}

/// Renders the item tree to a file, which does not depend on the platform
/// window's buffer, and reports whether the chrome and surface are in it.
void grabItems(QQuickWindow& window, const char* name) {
  const char* directory = std::getenv("RNQNX_JS_THREAD_SHOTS");
  if (directory == nullptr || *directory == '\0') return;
  const QSharedPointer<QQuickItemGrabResult> grabbed = window.contentItem()->grabToImage();
  if (grabbed.isNull()) {
    result(std::string("item grab ") + name + " failed (no result)");
    return;
  }
  bool ready = false;
  QObject::connect(grabbed.data(), &QQuickItemGrabResult::ready, [&] { ready = true; });
  try {
    waitUntil([&] { return ready; }, 3000, "item grab ready");
  } catch (const TestFailure&) {
    result(std::string("item grab ") + name + " did not finish in 3 s");
    return;
  }
  const QString path = QDir(QString::fromUtf8(directory)).filePath(QString::fromUtf8(name) + QStringLiteral(".png"));
  const QImage image = grabbed->image();
  result(std::string("item grab ") + name + " " + std::to_string(image.width()) + "x" +
         std::to_string(image.height()) + (image.save(path) ? " " : " (write failed) ") +
         path.toStdString() + " exposed=" + (window.isExposed() ? "yes" : "no"));
}

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
  mustRun(host, "answer");
  EXPECT(readNumber(host, "answer") == 42.0);
  EXPECT(!runScript(host, "boom"));
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
  mustRun(fixture.host(), "scroll-tree");
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
  // The callback reports the thread it runs on, through a native function, so
  // the check does not depend on which thread pumps events.
  std::atomic<int> timerThread{0};  // 1 = JS thread, 2 = elsewhere
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    runtime.global().setProperty(
        runtime, "recordThread",
        facebook::jsi::Function::createFromHostFunction(
            runtime, facebook::jsi::PropNameID::forAscii(runtime, "recordThread"), 0,
            [&](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
              timerThread = host.onJsThread() ? 1 : 2;
              return facebook::jsi::Value::undefined();
            }));
  });
  mustRun(host, "timer");
  waitUntil([&] { return readNumber(host, "fired") == 1.0; }, 2000, "timer fired");
  EXPECT(timerThread.load() == 1);

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
  EXPECT(installDimensionsRecorder(host));
  const std::string spin = scriptSource("spin-short");
  std::atomic<bool> done{false};
  host.postToJsThread([&] {
    host.evaluate(spin, "spin-short.js");
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
  // The posted calls ran once JavaScript was free: it was told the new size.
  waitUntil([&] { return readNumber(host, "dimsWidth") == 800.0; }, 2000,
            "JavaScript saw the window size");
  EXPECT(readNumber(host, "dimsHeight") == 600.0);
}

void blockedJavaScriptLeavesTheGuiThreadResponsive() {
  Fixture fixture;
  fixture.start();
  auto& host = fixture.host();
  EXPECT(installDimensionsRecorder(host));
  mustRun(host, "scroll-tree");
  mustRun(host, "touch-recorder");
  QQuickItem* scroll = fixture.layer().itemForTag(601);
  EXPECT(scroll != nullptr);
  EXPECT(scroll->property("contentHeight").toDouble() > scroll->height());
  QQuickItem* filler = fixture.layer().itemForTag(603);
  EXPECT(filler != nullptr);
  EXPECT(filler->width() == 640.0);

  setNumber(host, "RNQNX_BLOCK_MS", kBlockMs);
  const std::string block = scriptSource("block");
  std::atomic<bool> blockDone{false};
  std::atomic<int> queuedRan{0};
  // The posted work captures the locals above by reference. If a check throws
  // while JavaScript is still blocked, leave only after both ran.
  struct DrainOnExit {
    std::atomic<bool>& done;
    std::atomic<int>& queued;
    ~DrainOnExit() {
      QElapsedTimer wait;
      wait.start();
      while ((!done.load() || queued.load() == 0) && wait.elapsed() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(2);
      }
    }
  } drain{blockDone, queuedRan};
  QElapsedTimer blocked;
  host.postToJsThread([&] {
    host.evaluate(block, "block.js");
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
  qint64 panMs = 0;
  double panFrom = 0;
  double panTo = 0;
  {
    QElapsedTimer action;
    action.start();
    const double before = scroll->property("contentY").toDouble();
    const QPointF start = scroll->mapToScene(QPointF(100, 90));
    sendMouse(fixture.window(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    const int step = QGuiApplication::styleHints()->startDragDistance() + 6;
    for (int i = 1; i <= 5; ++i) {
      sendMouse(fixture.window(), QEvent::MouseMove, start - QPointF(0, i * step), Qt::NoButton,
                Qt::LeftButton);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    const double during = scroll->property("contentY").toDouble();
    sendMouse(fixture.window(), QEvent::MouseButtonRelease, start - QPointF(0, 5 * step),
              Qt::LeftButton, Qt::NoButton);
    panMs = action.elapsed();
    panFrom = before;
    panTo = during;
    elapsedOk("pan finished inside the block");
    if (!(during > before + 10.0)) {
      std::ostringstream detail;
      detail << "contentY " << before << " -> " << during << ", contentHeight "
             << scroll->property("contentHeight").toDouble() << ", height " << scroll->height()
             << ", interactive " << scroll->property("interactive").toBool() << ", start "
             << start.x() << "," << start.y() << ", z " << scroll->z() << " visible "
             << scroll->isVisible();
      fail("pan tracked while JavaScript was blocked", __FILE__, __LINE__, detail.str());
    }
    EXPECT(panMs < kResponsiveMs);
    grab(fixture.window(), "flag-on-after-pan-mid-block");
  }

  // Touch: a press, a move and a release on a React view reach the dispatcher
  // while the runtime is busy. JavaScript sees them once the block ends.
  {
    const QPointF spot = filler->mapToScene(QPointF(400, 300));
    sendMouse(fixture.window(), QEvent::MouseButtonPress, spot, Qt::LeftButton, Qt::LeftButton);
    sendMouse(fixture.window(), QEvent::MouseMove, spot + QPointF(0, 12), Qt::NoButton,
              Qt::LeftButton);
    sendMouse(fixture.window(), QEvent::MouseButtonRelease, spot + QPointF(0, 12),
              Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    elapsedOk("touch dispatch finished inside the block");
  }
  const int dispatched = fixture.dispatcher().dispatchedCount();
  EXPECT(dispatched >= 3);

  qint64 minimizeMs = 0;
  qint64 closeMs = 0;
#if RNQNX_ENABLE_WINDOW_CHROME
  {
    auto& chrome = fixture.chrome();
    QStringList triggered;
    QStringList unavailable;
    QObject::connect(&chrome, &rnqnx::window::WindowChrome::actionTriggered,
                     [&](const QString& action) { triggered << action; });
    QObject::connect(&chrome, &rnqnx::window::WindowChrome::actionUnavailable,
                     [&](const QString& action, const QString&) { unavailable << action; });
    // A real press and release on the control item, as a pointer would send.
    const auto click = [&](const char* control) {
      QQuickItem* item = findItemNamed(
          chrome.chromeItem(), QStringLiteral("rnqnxWindowControl_") + QLatin1String(control));
      EXPECT(item != nullptr);
      const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
      sendMouse(fixture.window(), QEvent::MouseButtonPress, centre, Qt::LeftButton,
                Qt::LeftButton);
      sendMouse(fixture.window(), QEvent::MouseButtonRelease, centre, Qt::LeftButton,
                Qt::NoButton);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    };
    // Qt's QNX plugin cannot minimize a native window; the chrome releases it
    // instead (WindowHost.h). Elsewhere the window state changes.
    const bool releasesWindow = QGuiApplication::platformName() == QLatin1String("qnx");
    QElapsedTimer action;
    action.start();
    click("minimize");
    minimizeMs = action.elapsed();
    EXPECT(minimizeMs < kResponsiveMs);
    elapsedOk("minimize finished inside the block");
    EXPECT(triggered == QStringList{QStringLiteral("minimize")});
    EXPECT(unavailable.isEmpty());
    bool swapped = false;
    const auto swapConnection =
        QObject::connect(&fixture.window(), &QQuickWindow::frameSwapped, [&] { swapped = true; });
    if (releasesWindow) {
      EXPECT(!fixture.window().isVisible());
      fixture.window().show();
    } else {
      EXPECT(fixture.window().visibility() == QWindow::Minimized);
      fixture.window().showNormal();
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    EXPECT(fixture.window().isVisible());
    // A grab right after the native window comes back can be empty; let the
    // first frame go out.
    try {
      waitUntil([&] { return swapped; }, 1500, "first frame after the restore");
    } catch (const TestFailure&) {
      result("no frameSwapped within 1500 ms of the restore");
    }
    QObject::disconnect(swapConnection);
    elapsedOk("window restored inside the block");
    grab(fixture.window(), "flag-on-after-restore-first-frame-mid-block");
    {
      QElapsedTimer settle;
      settle.start();
      while (settle.elapsed() < 400) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    elapsedOk("window settled inside the block");
    grab(fixture.window(), "flag-on-after-restore-400ms-mid-block");
    grabItems(fixture.window(), "flag-on-after-restore-item-tree-mid-block");

    action.restart();
    click("close");
    closeMs = action.elapsed();
    EXPECT(closeMs < kResponsiveMs);
    elapsedOk("close finished inside the block");
    EXPECT((triggered == QStringList{QStringLiteral("minimize"), QStringLiteral("close")}));
    EXPECT(!fixture.window().isVisible());
  }
#endif

  // Posted GUI-handler calls return at once even though the runtime is busy.
  qint64 resizeMs = 0;
  {
    QElapsedTimer action;
    action.start();
    host.resizeSurface(kSurfaceId, 700.0F, 500.0F);
    host.setWindowSize(700.0F, 500.0F, 1.0F);
    resizeMs = action.elapsed();
    EXPECT(resizeMs < kResponsiveMs);
    elapsedOk("resize calls returned inside the block");
  }

  // Keep the loop turning to the middle of the block. A 10 ms heartbeat that
  // skipped most of it would mean the GUI thread was stalled.
  waitUntil([&] { return blocked.elapsed() >= kBlockMs / 2; }, kBlockMs, "reached mid-block");
  EXPECT(!blockDone.load());
  const qint64 sampled = blocked.elapsed();
  const int beatsAtSample = beats;
  EXPECT(beatsAtSample >= sampled / 40);
  waitUntil([&] { return blockDone.load(); }, kBlockMs + 3000, "the block ended");
  EXPECT(blocked.elapsed() >= kBlockMs - 100);
  waitUntil([&] { return queuedRan.load() != 0; }, 2000, "queued work ran after the block");
  EXPECT(queuedRan.load() == 1);
  // The posted calls were not lost: JavaScript saw the size and the surface
  // was laid out at it, now that JavaScript is free.
  waitUntil([&] { return readNumber(host, "dimsWidth") == 700.0; }, 2000,
            "JavaScript saw the window size");
  EXPECT(readNumber(host, "dimsHeight") == 500.0);
  waitUntil([&] { return filler->width() == 700.0 && filler->height() == 500.0; }, 2000,
            "the surface was laid out at the new size");
  // The touch events dispatched during the block reached JavaScript after it,
  // none lost, the first one a start.
  try {
    waitUntil(
        [&] {
          runScript(host, "touch-check");
          return readNumber(host, "touchEnds") >= 2.0;
        },
        5000, "touch events reached JavaScript after the block");
  } catch (const TestFailure&) {
    std::ostringstream detail;
    detail << "native dispatched " << dispatched << ", JavaScript saw "
           << readNumber(host, "touchCount") << " (ends " << readNumber(host, "touchEnds") << ")";
    fail("touch events reached JavaScript after the block", __FILE__, __LINE__, detail.str());
  }
  const double touchCount = readNumber(host, "touchCount");
  // Fabric coalesces touchMove events that wait in its queue for the same view
  // (EventQueue::enqueueUniqueEvent), so a long block delivers one move per
  // view and every start and end. Two gestures went in: the pan on the scroll
  // view (a press, five moves, a release) and the tap on the filler (a press,
  // a move, a release).
  EXPECT(readNumber(host, "touchStarts") == 2.0);
  EXPECT(readNumber(host, "touchEnds") == 2.0);
  EXPECT(readNumber(host, "touchMoves") >= 2.0);
  EXPECT(readNumber(host, "touchMoves") <= 6.0);
  EXPECT(readNumber(host, "touchAfterBlock") == 1.0);
  EXPECT(readNumber(host, "touchOrdered") == 1.0);
  std::ostringstream summary;
  summary << "flag=on blockMs=" << kBlockMs << " blocked=" << blocked.elapsed()
          << "ms heartbeats@" << sampled << "ms=" << beatsAtSample << " total=" << beats
          << " panMs=" << panMs << " contentY=" << panFrom << "->" << panTo
          << " minimizeMs=" << minimizeMs << " closeMs=" << closeMs << " resizeMs=" << resizeMs
          << " touchDispatchedNatively=" << dispatched << " touchSeenByJs=" << touchCount
          << " (starts " << readNumber(host, "touchStarts") << ", moves "
          << readNumber(host, "touchMoves") << " coalesced from 6, ends "
          << readNumber(host, "touchEnds") << ")"
          << " responsiveBoundMs=" << kResponsiveMs;
  result(summary.str());
  std::cout << "blocked=" << blocked.elapsed() << "ms beats@" << sampled << "ms=" << beatsAtSample
            << " total=" << beats << std::endl;
}

/// The control for the case above: the same block with the flag off. Everything
/// the GUI is sent while JavaScript spins has to wait for it.
void blockedJavaScriptFreezesTheGuiThreadWithTheFlagOff() {
  Fixture fixture(false);
  fixture.start();
  auto& host = fixture.host();
  mustRun(host, "scroll-tree");
  QQuickItem* scroll = fixture.layer().itemForTag(601);
  EXPECT(scroll != nullptr);
  setNumber(host, "RNQNX_BLOCK_MS", kBlockMs);
  const std::string block = scriptSource("block");

  QElapsedTimer clock;
  int beats = 0;
  qint64 firstBeatAt = -1;
  qint64 lastBeatAt = -1;
  QTimer heartbeat;
  heartbeat.setInterval(10);
  heartbeat.setTimerType(Qt::PreciseTimer);
  QObject::connect(&heartbeat, &QTimer::timeout, [&] {
    ++beats;
    lastBeatAt = clock.elapsed();
    if (firstBeatAt < 0) firstBeatAt = lastBeatAt;
  });

  // Input is queued the way the platform queues it while the loop is busy: as
  // posted events, delivered when something returns to the event loop.
  const double contentBefore = scroll->property("contentY").toDouble();
  const QPointF panStart = scroll->mapToScene(QPointF(100, 90));
  const int step = QGuiApplication::styleHints()->startDragDistance() + 6;
  const auto post = [&](QEvent::Type type, QPointF point, Qt::MouseButton button,
                        Qt::MouseButtons buttons) {
    QCoreApplication::postEvent(
        &fixture.window(), new QMouseEvent(type, point, point, button, buttons, Qt::NoModifier));
  };
  post(QEvent::MouseButtonPress, panStart, Qt::LeftButton, Qt::LeftButton);
  for (int i = 1; i <= 5; ++i) {
    post(QEvent::MouseMove, panStart - QPointF(0, i * step), Qt::NoButton, Qt::LeftButton);
  }
  post(QEvent::MouseButtonRelease, panStart - QPointF(0, 5 * step), Qt::LeftButton, Qt::NoButton);

  qint64 closeTriggeredAt = -1;
  bool closeQueued = false;
#if RNQNX_ENABLE_WINDOW_CHROME
  QQuickItem* closeItem = findItemNamed(fixture.chrome().chromeItem(),
                                        QStringLiteral("rnqnxWindowControl_close"));
  EXPECT(closeItem != nullptr);
  QObject::connect(&fixture.chrome(), &rnqnx::window::WindowChrome::actionTriggered,
                   [&](const QString& action) {
                     if (action == QLatin1String("close")) closeTriggeredAt = clock.elapsed();
                   });
  const QPointF centre =
      closeItem->mapToScene(QPointF(closeItem->width() / 2, closeItem->height() / 2));
  post(QEvent::MouseButtonPress, centre, Qt::LeftButton, Qt::LeftButton);
  post(QEvent::MouseButtonRelease, centre, Qt::LeftButton, Qt::NoButton);
  closeQueued = true;
#endif

  clock.start();
  heartbeat.start();
  EXPECT(host.evaluate(block, "block.js"));
  const qint64 blockedFor = clock.elapsed();
  EXPECT(blockedFor >= kBlockMs - 100);
  // Nothing the GUI was sent ran during the block: this is the limit the flag
  // removes (ADR-0016).
  const int beatsDuringBlock = beats;
  const double contentAtEnd = scroll->property("contentY").toDouble();
  EXPECT(beatsDuringBlock <= 1);
  EXPECT(contentAtEnd == contentBefore);
  EXPECT(closeTriggeredAt < 0);
  EXPECT(fixture.window().isVisible());
  // Back in the event loop everything queued runs, so the events were not lost,
  // only late.
  if (closeQueued) {
    waitUntil([&] { return closeTriggeredAt >= 0; }, 5000,
              "queued close click ran after the block");
    EXPECT(closeTriggeredAt >= kBlockMs - 100);
  }
  waitUntil([&] { return scroll->property("contentY").toDouble() > contentBefore + 10.0; }, 5000,
            "queued pan ran after the block");
  std::ostringstream summary;
  summary << "flag=off blockMs=" << kBlockMs << " blocked=" << blockedFor
          << "ms heartbeatsDuringBlock=" << beatsDuringBlock << " firstHeartbeatAt=" << firstBeatAt
          << "ms lastHeartbeatAt=" << lastBeatAt << "ms contentYAtBlockEnd=" << contentAtEnd
          << " closeActionAt=" << closeTriggeredAt << "ms (queued click ran only after the block)";
  result(summary.str());
}

void teardownWithQueuedWorkIsClean() {
  for (int round = 0; round < 3; ++round) {
    std::shared_ptr<rnqnx::runtime::QtRuntimeTaskQueue> queue;
    std::atomic<bool> stop{false};
    std::atomic<bool> sawClosed{false};
    std::thread producer;
    {
      Fixture fixture;
      fixture.start();
      auto& host = fixture.host();
      host.installTimers();
      mustRun(host, "scroll-tree");
      mustRun(host, "interval");
      queue = host.taskQueue();
      producer = std::thread([&] {
        while (!stop.load()) {
          try {
            queue->post([] {});
          } catch (const std::exception&) {
            sawClosed = true;  // the contract after close is a throw, not a crash
            return;
          }
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      });
      QThread::msleep(30);
      // Work queued right before destruction: a commit whose mount would reach
      // the GUI thread after the runtime stopped, if shutdown did not order it.
      const std::string lastCommit = scriptSource("last-commit");
      host.postToJsThread([&host, lastCommit] { host.evaluate(lastCommit, "last-commit.js"); });
      // The fixture, and with it the host, is destroyed here while the
      // producer is still posting.
    }
    producer.join();
    stop = true;
    EXPECT(sawClosed.load());
    // A late post to the closed queue throws; it never reaches the freed runtime.
    bool threw = false;
    try {
      queue->post([] {});
    } catch (const std::runtime_error&) {
      threw = true;
    }
    EXPECT(threw);
  }
}

/// Worklets, Reanimated, Gesture Handler and AppState read the runtime from the
/// GUI thread. In this mode they have to say so, by name, instead of racing.
void unmovedLibrariesRefuseWithAClearError() {
  [[maybe_unused]] Fixture fixture;
  fixture.start();
  [[maybe_unused]] auto& host = fixture.host();
  [[maybe_unused]] const auto expectRefusal = [&](const char* library,
                                                  const std::function<void()>& attempt) {
    bool threw = false;
    std::string message;
    try {
      attempt();
    } catch (const std::exception& error) {
      threw = true;
      message = error.what();
    }
    if (!threw || message.find("JS-thread mode") == std::string::npos ||
        message.find("ADR-0021") == std::string::npos ||
        message.find(library) == std::string::npos) {
      fail(library, __FILE__, __LINE__, threw ? message : "install did not throw");
    }
    result(std::string("refusal ") + library + ": " + message);
  };
  int refusals = 0;
#if defined(RNQNX_JS_THREAD_TEST_WORKLETS)
  expectRefusal("Worklets",
                [&] { rnqnx::worklets::QtWorkletsPlatform::install(host, &fixture.window()); });
  ++refusals;
#endif
#if defined(RNQNX_JS_THREAD_TEST_REANIMATED)
  expectRefusal("Reanimated", [&] { rnqnx::reanimated::installReanimated(host, nullptr); });
  ++refusals;
#endif
#if defined(RNQNX_JS_THREAD_TEST_GESTURE)
  expectRefusal("Gesture Handler", [&] {
    rnqnx::gesture::QtGesturePlatform::install(host, fixture.adapter(), fixture.layer(),
                                               fixture.window(), nullptr);
  });
  ++refusals;
#endif
#if RNQNX_ENABLE_APP_STATE
  host.installTurboModuleBinding();
  mustRun(host, "turbo-lookup");
  const std::string appState = readEntry(host, "lookups", "AppState");
  if (appState.find("ADR-0021") == std::string::npos ||
      appState.find("AppState") == std::string::npos) {
    fail("AppState lookup refuses", __FILE__, __LINE__, appState);
  }
  result("refusal AppState: " + appState);
  ++refusals;
#endif
  if (refusals == 0) result("no refusing library is compiled into this build");
}

/// The modules that did not need the GUI thread: they resolve on the JS thread
/// and complete there.
void turboModulesResolveAndCompleteOnTheJsThread() {
  Fixture fixture;
  fixture.start();
  auto& host = fixture.host();
  host.installTurboModuleBinding();
  mustRun(host, "turbo-lookup");
  for (const char* name : {"FileReaderModule", "ImageLoader", "WebSocketModule"}) {
    const std::string found = readEntry(host, "lookups", name);
    if (found != "ok") fail(name, __FILE__, __LINE__, "lookup gave " + found);
  }

  // FileReader reports its documented answer through a Promise settled on the
  // JS thread.
  mustRun(host, "file-reader");
  waitUntil([&] { return readString(host, "fileReaderResult") != "pending"; }, 2000,
            "FileReader promise settled");
  const std::string fileReader = readString(host, "fileReaderResult");
  EXPECT(fileReader.rfind("rejected: FileReaderModule is not implemented", 0) == 0);
  result("FileReader readAsText -> " + fileReader);

  // Image loader: decode runs on the thread pool, the Promise settles on JS.
  const QString imagePath = QDir::temp().filePath(QStringLiteral("rnqnx-js-thread-probe.png"));
  QImage probe(37, 23, QImage::Format_RGB32);
  probe.fill(Qt::darkCyan);
  EXPECT(probe.save(imagePath));
  setString(host, "RNQNX_IMAGE_URI", QUrl::fromLocalFile(imagePath).toString().toStdString());
  mustRun(host, "image-size");
  waitUntil([&] { return readString(host, "imageResult") != "pending"; }, 10000,
            "image getSize promise settled");
  const std::string imageResult = readString(host, "imageResult");
  QFile::remove(imagePath);
  if (imageResult != "resolved") fail("image getSize resolved", __FILE__, __LINE__, imageResult);
  EXPECT(readNumber(host, "imageWidth") == 37.0);
  EXPECT(readNumber(host, "imageHeight") == 23.0);
  result("ImageLoader getSize(37x23 png) -> 37x23, settled on the JS thread");
}

#if defined(RNQNX_JS_THREAD_TEST_WEBSOCKET)
/// A loopback server on the GUI thread echoes text. The module is built on the
/// JS thread and its transport lives on the GUI thread.
void webSocketModuleRoundTripsWithTheJsThread() {
  Fixture fixture;
  fixture.start();
  auto& host = fixture.host();
  host.installTurboModuleBinding();

  QWebSocketServer server(QStringLiteral("js-thread-echo"), QWebSocketServer::NonSecureMode);
  EXPECT(server.listen(QHostAddress::LocalHost, 0));
  QObject::connect(&server, &QWebSocketServer::newConnection, [&server] {
    QWebSocket* peer = server.nextPendingConnection();
    QObject::connect(peer, &QWebSocket::textMessageReceived, peer, [peer](const QString& text) {
      peer->sendTextMessage(QStringLiteral("echo:") + text);
    });
    QObject::connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
  });
  host.runOnJsThread([&] {
    auto& runtime = host.runtime();
    runtime.global().setProperty(
        runtime, "RNQNX_ON_JS",
        facebook::jsi::Function::createFromHostFunction(
            runtime, facebook::jsi::PropNameID::forAscii(runtime, "RNQNX_ON_JS"), 0,
            [&](facebook::jsi::Runtime&, const facebook::jsi::Value&, const facebook::jsi::Value*,
                size_t) -> facebook::jsi::Value {
              return facebook::jsi::Value(host.onJsThread());
            }));
  });
  setString(host, "RNQNX_WS_URL", "ws://127.0.0.1:" + std::to_string(server.serverPort()));
  mustRun(host, "websocket");
  const auto reached = [&](const char* flag) {
    return [&host, flag] {
      runScript(host, "ws-summary");
      return readNumber(host, flag) == 1.0;
    };
  };
  waitUntil(reached("wsOpened"), 10000, "websocketOpen reached JavaScript");
  mustRun(host, "ws-send");
  waitUntil(reached("wsEcho"), 10000, "echoed message reached JavaScript");
  mustRun(host, "ws-close");
  waitUntil(reached("wsClosed"), 10000, "websocketClosed reached JavaScript");
  EXPECT(readNumber(host, "wsFailed") == 0.0);
  EXPECT(readNumber(host, "wsAllOnJs") == 1.0);
  result("WebSocketModule connect, send, echo, close over loopback: events delivered on the JS "
         "thread");
}
#endif

#if defined(RNQNX_JS_THREAD_TEST_NITRO)
/// The Nitro runtime installs on the JS thread with the Qt dispatcher, and the
/// dispatcher's owner is that thread.
void nitroInstallerAndDispatcherUseTheJsThread() {
  using namespace margelo::nitro;
  Fixture fixture;
  fixture.start();
  auto& host = fixture.host();
  auto dispatcher = std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host.taskQueue());
  host.runOnJsThread([&] { rnqnx::nitro::installQnxNitroRuntime(host.runtime(), dispatcher); });
  EXPECT(!dispatcher->isOnDispatcher());  // this is the GUI thread
  bool onDispatcherOnJs = false;
  host.runOnJsThread([&] { onDispatcherOnJs = dispatcher->isOnDispatcher(); });
  EXPECT(onDispatcherOnJs);
  bool syncRejected = false;
  try {
    dispatcher->runSync([] {});
  } catch (const std::runtime_error&) {
    syncRejected = true;
  }
  EXPECT(syncRejected);
  mustRun(host, "nitro-check");
  EXPECT(readString(host, "nitroProxy") != "undefined");
  mustRun(host, "nitro-setup");

  // A native thread calls a JS callback asynchronously: it runs on the JS thread.
  std::optional<Callback<double()>> callback;
  host.runOnJsThread([&] {
    callback.emplace(JSIConverter<Callback<double()>>::fromJSI(
        host.runtime(), host.runtime().global().getProperty(host.runtime(), "callback")));
  });
  std::optional<std::future<double>> asyncResult;
  bool workerOffDispatcher = false;
  std::thread worker([&] {
    workerOffDispatcher = !dispatcher->isOnDispatcher();
    asyncResult.emplace(callback->callAsync().await());
  });
  worker.join();
  EXPECT(workerOffDispatcher);
  waitUntil(
      [&] { return asyncResult->wait_for(std::chrono::seconds(0)) == std::future_status::ready; },
      5000, "async Nitro callback completed");
  EXPECT(asyncResult->get() == 42.0);
  EXPECT(readNumber(host, "calls") == 1.0);
  EXPECT(readNumber(host, "microtasks") == 1.0);

  // A native Promise settled from a worker resolves JavaScript on the JS thread.
  std::optional<Promise<double>> pending;
  host.runOnJsThread([&] {
    pending.emplace(Promise<double>::pending());
    host.runtime().global().setProperty(host.runtime(), "nativePending",
                                        JSIConverter<Promise<double>>::toJSI(host.runtime(), *pending));
  });
  mustRun(host, "nitro-resolve");
  std::thread resolver([&] { pending->resolve(42); });
  resolver.join();
  waitUntil([&] { return readNumber(host, "resolved") == 42.0; }, 5000,
            "native Promise resolved JS");
  callback.reset();
  pending.reset();
  result("Nitro installQnxNitroRuntime on the JS thread: async callback and native Promise round "
         "trip");
}
#endif

struct TestCase {
  const char* name;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  std::vector<std::string> only;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--only" && i + 1 < argc) only.emplace_back(argv[++i]);
  }
  const std::vector<TestCase> tests{
      {"jsRunsOnItsOwnThread", jsRunsOnItsOwnThread},
      {"mountsAreAppliedOnTheGuiThread", mountsAreAppliedOnTheGuiThread},
      {"timersAndCompletionsRunOnTheJsThread", timersAndCompletionsRunOnTheJsThread},
      {"resizeAndWindowSizeDoNotWaitForJavaScript", resizeAndWindowSizeDoNotWaitForJavaScript},
      {"blockedJavaScriptLeavesTheGuiThreadResponsive",
       blockedJavaScriptLeavesTheGuiThreadResponsive},
      {"blockedJavaScriptFreezesTheGuiThreadWithTheFlagOff",
       blockedJavaScriptFreezesTheGuiThreadWithTheFlagOff},
      {"teardownWithQueuedWorkIsClean", teardownWithQueuedWorkIsClean},
      {"unmovedLibrariesRefuseWithAClearError", unmovedLibrariesRefuseWithAClearError},
      {"turboModulesResolveAndCompleteOnTheJsThread",
       turboModulesResolveAndCompleteOnTheJsThread},
#if defined(RNQNX_JS_THREAD_TEST_WEBSOCKET)
      {"webSocketModuleRoundTripsWithTheJsThread", webSocketModuleRoundTripsWithTheJsThread},
#endif
#if defined(RNQNX_JS_THREAD_TEST_NITRO)
      {"nitroInstallerAndDispatcherUseTheJsThread", nitroInstallerAndDispatcherUseTheJsThread},
#endif
  };
  std::cout << "[INFO] platform=" << QGuiApplication::platformName().toStdString()
            << " blockMs=" << kBlockMs << " responsiveMs=" << kResponsiveMs << std::endl;
  int failed = 0;
  int ran = 0;
  for (const auto& test : tests) {
    if (!only.empty() &&
        std::find(only.begin(), only.end(), std::string(test.name)) == only.end()) {
      continue;
    }
    ++ran;
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
    std::cerr << failed << " JS-thread test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All " << ran << " JS-thread tests passed.\n";
  return EXIT_SUCCESS;
}
