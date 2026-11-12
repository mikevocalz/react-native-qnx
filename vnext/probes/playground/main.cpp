#include "react-native-qnx/views/QtTextInputQml.h"
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
#if RNQNX_ENABLE_WINDOW_CHROME
#include "WindowChrome.h"
#ifdef RNQNX_CONSUMER_WINDOW_HEADER
#include RNQNX_CONSUMER_WINDOW_HEADER
#endif
#endif
#include "react-native-qnx/views/QtQuickItemFactory.h"

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
#include <memory>
#include <sstream>
#include <stdexcept>

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
constexpr const char* kSwitchSource =
    "import QtQuick\n"
    "Rectangle {\n"
    "  id: root\n"
    "  property bool value: false\n"
    "  property bool disabled: false\n"
    "  property color trackOnColor: '#4cd964'\n"
    "  property color trackOffColor: '#ffffff'\n"
    "  property color thumbColor: '#ffffff'\n"
    "  visible: true\n"
    "  Rectangle {\n"
    "    id: track\n"
    "    anchors.fill: parent\n"
    "    radius: parent.height / 2\n"
    "    color: root.value ? root.trackOnColor : root.trackOffColor\n"
    "  }\n"
    "  Rectangle {\n"
    "    id: thumb\n"
    "    width: parent.height - 4\n"
    "    height: width\n"
    "    radius: width / 2\n"
    "    color: root.thumbColor\n"
    "    anchors.verticalCenter: parent.verticalCenter\n"
    "    x: root.value ? parent.width - width - 2 : 2\n"
    "  }\n"
    "  MouseArea {\n"
    "    anchors.fill: parent\n"
    "    enabled: !root.disabled\n"
    "    onClicked: {\n"
    "      root.value = !root.value;\n"
    "    }\n"
    "  }\n"
    "}\n";
constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 480;

}  // namespace

int main(int argc, char** argv) {
#if RNQNX_ENABLE_WINDOW_CHROME
  // Read before QGuiApplication strips -platform from argv (ADR-0016).
  const QStringList platformOptions = rnqnx::window::platformOptionsFromArguments(
      argc, argv, qEnvironmentVariable("QT_QPA_PLATFORM"));
#endif
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
#if RNQNX_ENABLE_FRAME_TRACE
  const QCommandLineOption traceOption(QStringLiteral("trace-json"),
      QStringLiteral("Export bounded native diagnostics after host shutdown (new file only)."),
      QStringLiteral("path"));
  parser.addOption(traceOption);
#endif
  parser.addOption(fullscreenOption);
#if RNQNX_ENABLE_WINDOW_CHROME
  const QCommandLineOption windowControlsOption(QStringLiteral("window-controls"),
      QStringLiteral("Title bar control layout: macos or windows."), QStringLiteral("layout"));
  const QCommandLineOption titleBarStyleOption(QStringLiteral("title-bar-style"),
      QStringLiteral("Title bar style: default, hidden or none."), QStringLiteral("style"));
  const QCommandLineOption windowSizeOption(QStringLiteral("window-size"),
      QStringLiteral("Initial outer window size, clamped to the screen."), QStringLiteral("WxH"));
  parser.addOption(windowControlsOption);
  parser.addOption(titleBarStyleOption);
  parser.addOption(windowSizeOption);
#endif
  parser.addOption(bundleOption);
  parser.addOption(screenshotOption);
  parser.process(app);

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

#if RNQNX_ENABLE_WINDOW_CHROME
  // The playground keeps its original chromeless window unless asked; a
  // consumer app uses the `window` object from its native-app.json.
  rnqnx::window::WindowChromeConfig chromeConfig;
  chromeConfig.titleBarStyle = rnqnx::window::TitleBarStyle::None;
  try {
#ifdef RNQNX_CONSUMER_WINDOW_HEADER
    chromeConfig = rnqnx::window::configuredWindowChrome();
#endif
    if (parser.isSet(windowControlsOption)) {
      chromeConfig.controls = rnqnx::window::parseControlsLayout(parser.value(windowControlsOption));
      if (!parser.isSet(titleBarStyleOption) &&
          chromeConfig.titleBarStyle == rnqnx::window::TitleBarStyle::None) {
        chromeConfig.titleBarStyle = rnqnx::window::TitleBarStyle::Default;
      }
    }
    if (parser.isSet(titleBarStyleOption)) {
      chromeConfig.titleBarStyle = rnqnx::window::parseTitleBarStyle(parser.value(titleBarStyleOption));
    }
    if (parser.isSet(windowSizeOption)) {
      const QStringList edges = parser.value(windowSizeOption).split(QLatin1Char('x'));
      bool widthOk = false;
      bool heightOk = false;
      const int width = edges.size() == 2 ? edges.at(0).toInt(&widthOk) : 0;
      const int height = edges.size() == 2 ? edges.at(1).toInt(&heightOk) : 0;
      if (!widthOk || !heightOk || width < 1 || height < 1 ||
          width > rnqnx::window::kMaxWindowEdge || height > rnqnx::window::kMaxWindowEdge) {
        throw std::invalid_argument("--window-size must be WxH with edges from 1 to 16384.");
      }
      chromeConfig.initialSize = QSize(width, height);
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr, "RNQNX_WINDOW_CONFIG_ERROR %s\n", error.what());
    return 2;
  }
#endif

  QQuickWindow window;
#ifdef RNQNX_MARATHON_HOST
  window.setTitle(QStringLiteral("React Native MarathonOS playground"));
#else
  window.setTitle(QStringLiteral("react-native-qnx playground"));
#endif
  if (identity.custom) window.setTitle(identity.displayName);
  window.setWidth(kWindowWidth);
  window.setHeight(kWindowHeight);
#if RNQNX_ENABLE_WINDOW_CHROME
  if (chromeConfig.initialSize && !parser.isSet(fullscreenOption)) {
    const QRect available = window.screen() != nullptr ? window.screen()->availableGeometry() : QRect();
    const QSize size = rnqnx::window::clampWindowSize(*chromeConfig.initialSize, available.size());
    window.resize(size);
    if (available.isValid()) {
      window.setPosition(available.center() - QPoint(size.width() / 2, size.height() / 2));
    }
  }
#endif
  window.setColor(QColor(Qt::white));
  if (parser.isSet(fullscreenOption)) {
    if (window.screen() == nullptr) {
      std::fprintf(stderr, "No screen is available for fullscreen mode\n");
      return 1;
    }
    window.setGeometry(window.screen()->geometry());
  }

  QQmlEngine engine;
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
      QStringLiteral("Switch"), QByteArray(kSwitchSource));

  QQuickItem* mountRoot = window.contentItem();
#if RNQNX_ENABLE_WINDOW_CHROME
  // Declared after the engine and before the mounting layer, so the layer
  // unmounts first and the chrome items go before the engine.
  std::unique_ptr<rnqnx::window::WindowChrome> chrome;
  if (!parser.isSet(fullscreenOption) &&
      chromeConfig.titleBarStyle != rnqnx::window::TitleBarStyle::None) {
    // The qnx QPA resizes the root window to the screen only once events run,
    // after the surface has started. Apply that size now so the content item
    // and the React surface match the window from the first frame.
    if (rnqnx::window::platformForcesFullScreen(QGuiApplication::platformName(), platformOptions) &&
        window.screen() != nullptr) {
      window.setGeometry(window.screen()->geometry());
    }
    try {
      chrome = std::make_unique<rnqnx::window::WindowChrome>(window, engine, chromeConfig,
          rnqnx::window::capabilitiesForPlatform(QGuiApplication::platformName(), platformOptions));
    } catch (const std::exception& error) {
      std::fprintf(stderr, "RNQNX_WINDOW_CHROME_ERROR %s\n", error.what());
      return 1;
    }
    mountRoot = &chrome->contentRoot();
    std::printf("RNQNX_WINDOW_CHROME platform=%s controls=%s style=%s inset=%g window=%dx%d\n",
        QGuiApplication::platformName().toUtf8().constData(),
        rnqnx::window::nameOf(chromeConfig.controls).toUtf8().constData(),
        rnqnx::window::nameOf(chromeConfig.titleBarStyle).toUtf8().constData(),
        chrome->layout().contentInset, window.width(), window.height());
    for (const QString& line : chrome->describeHiddenControls()) {
      std::printf("%s\n", line.toUtf8().constData());
    }
    QObject::connect(chrome.get(), &rnqnx::window::WindowChrome::actionTriggered,
        [](const QString& action) {
          std::printf("RNQNX_WINDOW_ACTION %s\n", action.toUtf8().constData());
          std::fflush(stdout);
        });
  }
#endif
  rnqnx::fabric::QtQuickMountingLayer layer(
      factory, *mountRoot, kSurfaceId);
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

#if RNQNX_ENABLE_INPUT_LIFECYCLE
  // Outlive the host: stopping its surface can still remove captured targets.
  rnqnx::fabric::QtTouchDispatcher touchDispatcher(layer, *mountRoot);
  adapter.setTouchDispatcher(&touchDispatcher);
  window.installEventFilter(&touchDispatcher);
#endif
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
#if RNQNX_ENABLE_INPUT_LIFECYCLE
  host.delegate().setTouchDispatcher(&touchDispatcher);
#endif
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
  } else {
    window.show();
  }
  // Without chrome the surface is the whole window, as before.
  QSizeF surfaceSize(window.width(), window.height());
#if RNQNX_ENABLE_WINDOW_CHROME
  if (chrome) surfaceSize = chrome->contentRoot().size();
#endif
  host.startSurface(
      kSurfaceId,
      identity.componentName.toStdString(),
      static_cast<float>(surfaceSize.width()),
      static_cast<float>(surfaceSize.height()));

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