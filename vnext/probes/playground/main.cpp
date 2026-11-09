#include "react-native-qnx/views/QtTextInputQml.h"
#include "react-native-qnx/views/QtSwitchQml.h"
// Runs apps/rn-playground in a real window.
//
// The host tests prove the same path under the offscreen platform plugin; this
// exists so a person can look at the result. It is also the shape the QNX
// runner will take once a target exists: create a window, build a host, load a
// bundle, start a surface, run the event loop.
//
// Usage:
//   rnqnx_playground_host [--bundle <path>] [--screenshot <path>] [--fullscreen]
//
// With --screenshot it renders, writes a PNG, and exits, which is how it stays
// useful on a machine with no display.

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/runtime/ApplicationIdentity.h"
#ifdef RNQNX_CONSUMER_APP_HEADER
#include RNQNX_CONSUMER_APP_HEADER
#endif
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/QtWheelDirectionFilter.h"
#if RNQNX_ENABLE_DESKTOP_WINDOW
#include "react-native-qnx/views/QtDesktopWindow.h"
#endif

#ifdef RNQNX_PLAYGROUND_NITRO_QNX
#include "QnxNitroRuntime.hpp"
#ifdef RNQNX_PLAYGROUND_NITRO_QT
#include "QtNitroDispatcher.hpp"
#endif
#endif

#include <QByteArray>
#include <QColor>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QString>
#include <QTimer>

#include <cstdio>
#include <algorithm>
#include <memory>
#include <sstream>

namespace {

constexpr facebook::react::SurfaceId kSurfaceId = 1;
constexpr const char* kViewSource =
    "import QtQuick\n"
    "import QtQuick.Shapes\n"
    "Item {\n"
    "  id: root\n"
    "  visible: true\n"
    "  clip: true\n"
    "  property real radius: 0\n"
    "  property real topLeftRadius: 0\n"
    "  property real topRightRadius: 0\n"
    "  property real bottomRightRadius: 0\n"
    "  property real bottomLeftRadius: 0\n"
    "  property color backgroundColor: \"transparent\"\n"
    "  property real borderWidth: 0\n"
    "  property color borderColor: \"transparent\"\n"
    "  property bool usePerCornerRadius: false\n"
    "  Rectangle {\n"
    "    anchors.fill: parent\n"
    "    visible: !root.usePerCornerRadius\n"
    "    color: root.backgroundColor\n"
    "    radius: root.radius\n"
    "    border.width: root.borderWidth\n"
    "    border.color: root.borderColor\n"
    "  }\n"
    "  Shape {\n"
    "    anchors.fill: parent\n"
    "    visible: root.usePerCornerRadius\n"
    "    ShapePath {\n"
    "      fillColor: root.backgroundColor\n"
    "      strokeColor: root.borderColor\n"
    "      strokeWidth: root.borderWidth\n"
    "      startX: root.topLeftRadius; startY: 0\n"
    "      PathLine { x: root.width - root.topRightRadius; y: 0 }\n"
    "      PathArc {\n"
    "        x: root.width;\n"
    "        y: root.topRightRadius;\n"
    "        radiusX: root.topRightRadius;\n"
    "        radiusY: root.topRightRadius;\n"
    "        useLargeArc: false;\n"
    "        direction: PathArc.Clockwise\n"
    "      }\n"
    "      PathLine { x: root.width; y: root.height - root.bottomRightRadius }\n"
    "      PathArc {\n"
    "        x: root.width - root.bottomRightRadius;\n"
    "        y: root.height;\n"
    "        radiusX: root.bottomRightRadius;\n"
    "        radiusY: root.bottomRightRadius;\n"
    "        useLargeArc: false;\n"
    "        direction: PathArc.Clockwise\n"
    "      }\n"
    "      PathLine { x: root.bottomLeftRadius; y: root.height }\n"
    "      PathArc {\n"
    "        x: 0;\n"
    "        y: root.height - root.bottomLeftRadius;\n"
    "        radiusX: root.bottomLeftRadius;\n"
    "        radiusY: root.bottomLeftRadius;\n"
    "        useLargeArc: false;\n"
    "        direction: PathArc.Clockwise\n"
    "      }\n"
    "      PathLine { x: 0; y: root.topLeftRadius }\n"
    "      PathArc {\n"
    "        x: root.topLeftRadius;\n"
    "        y: 0;\n"
    "        radiusX: root.topLeftRadius;\n"
    "        radiusY: root.topLeftRadius;\n"
    "        useLargeArc: false;\n"
    "        direction: PathArc.Clockwise\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n";
constexpr const char* kTextSource = "import QtQuick\nText { visible: true }\n";
constexpr const char* kImageSource =
    "import QtQuick\n"
    "Image {\n"
    "  id: root\n"
    "  visible: true\n"
    "  property var bridge: null\n"
    "  property string loadingSource: \"\"\n"
    "  Image {\n"
    "    id: loadingImage\n"
    "    objectName: \"loadingImage\"\n"
    "    anchors.fill: parent\n"
    "    source: root.loadingSource\n"
    "    visible: root.loadingSource !== \"\" &&\n"
    "             root.status !== Image.Ready &&\n"
    "             root.status !== Image.Error\n"
    "    z: -1\n"
    "  }\n"
    "  onStatusChanged: { if (bridge) bridge.notify(); }\n"
    "}\n";
// React Native's TextInput renders behind a single-line QML `TextInput` or a
// multi-line `TextEdit`, both hosted in an `Item` that provides a `text`
// property, `textChanged` signal, and an overlaid placeholder `Text`.
const char* kTextInputSource = rnqnx::views::textInputQml();
constexpr const char* kScrollViewSource =
    "import QtQuick\nFlickable { visible: true; clip: true }\n";
constexpr const char* kActivityIndicatorSource =
    "import QtQuick\n"
    "Item {\n"
    "  id: root\n"
    "  property color color: '#999999'\n"
    "  property bool animating: true\n"
    "  property bool hidesWhenStopped: true\n"
    "  property string size: 'small'\n"
    "  visible: animating || !hidesWhenStopped\n"
    "  Rectangle {\n"
    "    id: track\n"
    "    anchors.centerIn: parent\n"
    "    width: Math.min(parent.width, parent.height) * 0.8\n"
    "    height: width\n"
    "    radius: width / 2\n"
    "    color: 'transparent'\n"
    "    border.width: width * 0.1\n"
    "    border.color: root.color\n"
    "    Rectangle {\n"
    "      id: arm\n"
    "      x: track.width / 2 - width / 2\n"
    "      y: track.height / 2 - height\n"
    "      width: track.width * 0.12\n"
    "      height: track.height * 0.4\n"
    "      color: root.color\n"
    "      transformOrigin: Item.Bottom\n"
    "      NumberAnimation on rotation {\n"
    "        from: 0\n"
    "        to: 360\n"
    "        duration: 1000\n"
    "        loops: Animation.Infinite\n"
    "        running: root.animating\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n";
constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 480;

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
#ifdef RNQNX_MARATHON_HOST
  // Qt Wayland uses desktopFileName for xdg_toplevel app_id. Marathon's app
  // identity/manifest must ultimately match the upstream-approved runner.
  QCoreApplication::setApplicationName(QStringLiteral("rnqnx-marathon"));
  QGuiApplication::setDesktopFileName(
      QStringLiteral("org.rnqnx.marathon.playground"));
#else
  QCoreApplication::setApplicationName(QStringLiteral("rnqnx-playground"));
#endif

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Runs the React Native QNX playground in a window."));
  parser.addHelpOption();
  rnqnx::runtime::addApplicationIdentityOptions(parser);
  const QCommandLineOption bundleOption(
      QStringLiteral("bundle"),
      QStringLiteral("Path to the Metro bundle."),
      QStringLiteral("path"));
  const QCommandLineOption screenshotOption(
      QStringLiteral("screenshot"),
      QStringLiteral("Render once, write a PNG here, and exit."),
      QStringLiteral("path"));
  const QCommandLineOption fullscreenOption(
      QStringLiteral("fullscreen"),
      QStringLiteral("Fill the current monitor and use its size for the React surface."));
  const QCommandLineOption desktopOption(
      QStringLiteral("desktop"),
      QStringLiteral("Show native desktop title bar, menus and window controls (requires desktop build option)."));
  const QCommandLineOption systemFrameOption(
      QStringLiteral("system-window-frame"),
      QStringLiteral("Use the desktop window manager's frame with --desktop, retaining the app menu bar."));
  const QCommandLineOption invertWheelOption(
      QStringLiteral("invert-wheel"),
      QStringLiteral("Reverse wheel input for an affected device; dragging and touch are unchanged."));
#if RNQNX_ENABLE_FRAME_TRACE
  const QCommandLineOption traceOption(QStringLiteral("trace-json"),
      QStringLiteral("Export bounded native diagnostics after host shutdown (new file only)."),
      QStringLiteral("path"));
  parser.addOption(traceOption);
#endif
  parser.addOption(fullscreenOption);
  parser.addOption(desktopOption);
  parser.addOption(systemFrameOption);
  parser.addOption(invertWheelOption);
  parser.addOption(bundleOption);
  parser.addOption(screenshotOption);
  parser.process(app);
  if (parser.isSet(systemFrameOption) && !parser.isSet(desktopOption)) {
    std::fprintf(stderr, "RNQNX_DESKTOP_ERROR --system-window-frame requires --desktop\n");
    return 2;
  }
#if !RNQNX_ENABLE_DESKTOP_WINDOW
  if (parser.isSet(desktopOption)) {
    std::fprintf(stderr, "RNQNX_DESKTOP_UNAVAILABLE rebuild with RNQNX_ENABLE_DESKTOP_WINDOW=ON\n");
    return 2;
  }
#endif

  rnqnx::runtime::ApplicationIdentity identity;
  try {
#ifdef RNQNX_CONSUMER_APP_HEADER
    identity = rnqnx::runtime::configuredApplicationIdentity();
#endif
    identity = rnqnx::runtime::readApplicationIdentity(parser, identity);
    rnqnx::runtime::applyApplicationIdentity(identity);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "RNQNX_APP_IDENTITY_ERROR %s\n", error.what());
    return 2;
  }
  if (parser.isSet(QStringLiteral("describe-app"))) {
    std::printf("%s\n", rnqnx::runtime::describeApplicationIdentity(identity).constData());
    return 0;
  }

  const QString bundlePath = parser.isSet(bundleOption)
      ? parser.value(bundleOption)
#ifdef RNQNX_CONSUMER_APP_HEADER
      : QCoreApplication::applicationDirPath() + QStringLiteral("/app.bundle.js");
#else
      : QStringLiteral(RNQNX_PLAYGROUND_BUNDLE_PATH);
#endif

  QFile bundleFile(bundlePath);
  if (!bundleFile.open(QIODevice::ReadOnly)) {
    std::fprintf(
        stderr,
        "Could not read the bundle at %s\n",
        bundlePath.toUtf8().constData());
    return 1;
  }
  const QByteArray bundle = bundleFile.readAll();

  QQuickWindow window;
  rnqnx::views::QtWheelDirectionFilter wheelDirection(window, parser.isSet(invertWheelOption));
#ifdef RNQNX_MARATHON_HOST
  window.setTitle(QStringLiteral("React Native MarathonOS playground"));
#else
  window.setTitle(QStringLiteral("react-native-qnx playground"));
#endif
  if (identity.custom) window.setTitle(identity.displayName);
  window.setWidth(kWindowWidth);
  window.setHeight(kWindowHeight);
  window.setColor(QColor(Qt::white));
  if (parser.isSet(fullscreenOption)) {
    if (window.screen() == nullptr) {
      std::fprintf(stderr, "No screen is available for fullscreen mode\n");
      return 1;
    }
    window.setGeometry(window.screen()->geometry());
  }

  QQmlEngine engine;
  QQuickItem* surfaceRoot = window.contentItem();
#if RNQNX_ENABLE_DESKTOP_WINDOW
  std::unique_ptr<rnqnx::views::QtDesktopWindow> desktopWindow;
  if (parser.isSet(desktopOption)) {
    try {
      desktopWindow = std::make_unique<rnqnx::views::QtDesktopWindow>(engine, window,
          parser.isSet(systemFrameOption) ? rnqnx::views::WindowFrame::System
                                         : rnqnx::views::WindowFrame::Client);
      surfaceRoot = &desktopWindow->contentItem();
      if (!parser.isSet(fullscreenOption) && window.screen() != nullptr) {
        const QRect available = window.screen()->availableGeometry();
        window.resize(std::min(1100, available.width()), std::min(760, available.height()));
      }
    } catch (const std::exception& error) {
      std::fprintf(stderr, "RNQNX_DESKTOP_ERROR %s\n", error.what());
      return 2;
    }
  }
#endif
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(kViewSource));
  factory.registerType(QStringLiteral("Text"), QByteArray(kTextSource));
  factory.registerType(QStringLiteral("Image"), QByteArray(kImageSource));
  factory.registerType(
      QStringLiteral("ScrollView"), QByteArray(kScrollViewSource));
  factory.registerType(
      QStringLiteral("TextInput"), QByteArray(kTextInputSource));
  factory.registerType(
      QStringLiteral("ActivityIndicatorView"),
      QByteArray(kActivityIndicatorSource));
  factory.registerType(
      QStringLiteral("Switch"), QByteArray(rnqnx::views::switchQml()));

  rnqnx::fabric::QtQuickMountingLayer layer(
      factory, *surfaceRoot, kSurfaceId);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  adapter.mapComponent(QStringLiteral("Paragraph"), QStringLiteral("Text"));
  adapter.mapComponent(QStringLiteral("Image"), QStringLiteral("Image"));
  adapter.mapComponent(
      QStringLiteral("ScrollView"), QStringLiteral("ScrollView"));
  adapter.mapComponent(
      QStringLiteral("TextInput"), QStringLiteral("TextInput"));
  adapter.mapComponent(
      QStringLiteral("ActivityIndicatorView"),
      QStringLiteral("ActivityIndicatorView"));
  adapter.mapComponent(
      QStringLiteral("Switch"), QStringLiteral("Switch"));
  adapter.mapComponent(
      QStringLiteral("SafeAreaView"), QStringLiteral("View"));
  adapter.mapComponent(
      QStringLiteral("ModalHostView"), QStringLiteral("View"));

  // Baseline input is required for ordinary React controls in every consumer.
  // The lifecycle flag selects cancellation behavior inside the dispatcher.
  // Outlive the host: stopping its surface can still remove captured targets.
  rnqnx::fabric::QtTouchDispatcher touchDispatcher(layer, *surfaceRoot);
  adapter.setTouchDispatcher(&touchDispatcher);
  window.installEventFilter(&touchDispatcher);
#if RNQNX_ENABLE_FRAME_TRACE
  std::shared_ptr<rnqnx::runtime::RuntimeTrace> trace;
  if (parser.isSet(traceOption)) trace = std::make_shared<rnqnx::runtime::RuntimeTrace>(16384);
#endif
  const int result = [&]() {
#if RNQNX_ENABLE_FRAME_TRACE
  rnqnx::runtime::QtReactHost host(adapter, trace);
#else
  rnqnx::runtime::QtReactHost host(adapter);
#endif
  host.delegate().setTouchDispatcher(&touchDispatcher);
  std::printf("RNQNX_INPUT_READY lifecycle=%d\n", RNQNX_ENABLE_INPUT_LIFECYCLE);
  std::fflush(stdout);
#ifdef RNQNX_PLAYGROUND_NITRO_QNX
#ifdef RNQNX_PLAYGROUND_NITRO_QT
  rnqnx::nitro::installQnxNitroRuntime(
      host.runtime(), std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host.taskQueue()));
#else
  rnqnx::nitro::installQnxNitroRuntime(host.runtime());
#endif
#endif
  host.installTurboModuleBinding();
  host.installComponentProvider();
  host.installPlatformConstants();
  host.installTimers();

  if (!host.evaluate(bundle.toStdString(), bundlePath.toStdString())) {
    std::fprintf(
        stderr,
        "Bundle failed to evaluate: %s\n",
        host.lastError().toUtf8().constData());
    return 1;
  }

  if (parser.isSet(fullscreenOption)) {
    window.showFullScreen();
  } else if (parser.isSet(desktopOption)) {
    window.showNormal();
  } else {
    window.show();
  }
  host.startSurface(
      kSurfaceId,
      identity.componentName.toStdString(),
      static_cast<float>(surfaceRoot->width()),
      static_cast<float>(surfaceRoot->height()));

  // Window chrome is outside the Yoga viewport. Coalesce a width/height pair
  // into one surface constraint update, keeping every connection inside the
  // host's lifetime. This also handles ordinary window-manager resizes.
  QTimer surfaceResizeTimer;
  surfaceResizeTimer.setSingleShot(true);
  QObject::connect(surfaceRoot, &QQuickItem::widthChanged,
      &surfaceResizeTimer, [&surfaceResizeTimer]() { surfaceResizeTimer.start(0); });
  QObject::connect(surfaceRoot, &QQuickItem::heightChanged,
      &surfaceResizeTimer, [&surfaceResizeTimer]() { surfaceResizeTimer.start(0); });
  QObject::connect(&surfaceResizeTimer, &QTimer::timeout, [&host, surfaceRoot]() {
    if (surfaceRoot->width() > 0 && surfaceRoot->height() > 0 &&
        !host.resizeSurface(kSurfaceId, static_cast<float>(surfaceRoot->width()),
            static_cast<float>(surfaceRoot->height()))) {
      std::fprintf(stderr, "RNQNX_SURFACE_RESIZE_ERROR %s\n", host.lastError().toUtf8().constData());
    }
  });

  if (parser.isSet(screenshotOption)) {
    for (int i = 0; i < 20; ++i) {
      host.runtime().drainMicrotasks();
      host.pumpEvents();
    }
    if (!host.lastError().isEmpty()) {
      std::fprintf(
          stderr,
          "RNQNX_PLAYGROUND_RENDER_FAILED %s\n",
          host.lastError().toUtf8().constData());
      return 1;
    }
    const QImage frame = window.grabWindow();
    // Include React's surface cleanup in this one-shot trial while the runtime
    // is still available to report errors. Keep the captured frame unchanged.
    host.stopSurface(kSurfaceId);
    for (int i = 0; i < 20; ++i) {
      host.runtime().drainMicrotasks();
      host.pumpEvents();
    }
    // grabWindow and surface cleanup can process events. A valid Qt image
    // cannot turn a failed application or failed cleanup into a passing trial.
    if (!host.lastError().isEmpty()) {
      std::fprintf(
          stderr,
          "RNQNX_PLAYGROUND_RENDER_FAILED %s\n",
          host.lastError().toUtf8().constData());
      return 1;
    }
    if (frame.isNull() || !frame.save(parser.value(screenshotOption))) {
      std::fprintf(stderr, "Could not write the screenshot\n");
      return 1;
    }
    std::printf(
        "RNQNX_PLAYGROUND_SCREENSHOT %s %dx%d\n",
        parser.value(screenshotOption).toUtf8().constData(),
        frame.width(),
        frame.height());
    return 0;
  }

  // React schedules through microtasks and the RuntimeScheduler; with a running
  // event loop the host still has to drain the microtask queue, so this pumps
  // it alongside Qt's own events.
  QTimer drainTimer;
  QObject::connect(&drainTimer, &QTimer::timeout, [&host]() {
    host.runtime().drainMicrotasks();
  });
  drainTimer.start(16);

  return QGuiApplication::exec();
  }();
#if RNQNX_ENABLE_FRAME_TRACE
  if (trace) {
    try {
      const auto snapshot = trace->freeze();
      std::ostringstream encoded;
      rnqnx::runtime::RuntimeTrace::writeJson(snapshot, encoded);
      const auto bytes = encoded.str();
      QFile file(parser.value(traceOption));
      if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly,
                     QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
          file.write(bytes.data(), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size()) ||
          !file.flush()) {
        std::fprintf(stderr, "RNQNX_TRACE_EXPORT_FAILED\n");
        return 1;
      }
      std::printf("RNQNX_TRACE_EXPORTED records=%zu dropped=%llu\n", snapshot.events.size(),
                  static_cast<unsigned long long>(snapshot.dropped));
    } catch (const std::exception&) {
      std::fprintf(stderr, "RNQNX_TRACE_EXPORT_FAILED\n");
      return 1;
    }
  }
#endif
  return result;
}
