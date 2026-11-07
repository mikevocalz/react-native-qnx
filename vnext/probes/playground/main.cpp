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
constexpr const char* kTextInputSource = R"TEXTINPUT(import QtQuick
FocusScope {
  id: root
  visible: true
  clip: false

  property string text: ""
  property string placeholderText: ""
  property color placeholderTextColor: "#999999"
  property bool readOnly: false
  property bool multiline: false
  property int echoMode: TextInput.Normal
  property int maximumLength: 0
  property font font
  property color color

  signal submitEditing()
  signal keyPressed()
  signal selectionChanged()
  property string lastKeyText: ""
  property int selectionStart: 0
  property int selectionEnd: 0

  function mapKeyText(event) {
    if (event.key === Qt.Key_Backspace) return "";
    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) return "\n";
    if (event.key === Qt.Key_Tab) return "\t";
    if (event.key === Qt.Key_Escape) return "\x1B";
    return event.text;
  }

  TextInput {
    id: input
    objectName: "qnx-text-input"
    anchors.fill: parent
    visible: !root.multiline
    focus: !root.multiline
    readOnly: root.readOnly
    echoMode: root.echoMode
    maximumLength: root.maximumLength > 0 ? root.maximumLength : 32767
    font: root.font
    color: root.color
    onTextChanged: {
      if (root.text !== input.text) {
        root.text = input.text;
      }
    }
    onAccepted: { root.submitEditing(); }
    onCursorPositionChanged: {
      root.selectionStart = input.selectionStart;
      root.selectionEnd = input.selectionEnd;
      root.selectionChanged();
    }
    Keys.onPressed: {
      root.lastKeyText = root.mapKeyText(event);
      root.keyPressed();
    }
  }

  TextEdit {
    id: edit
    objectName: "qnx-text-edit"
    anchors.fill: parent
    visible: root.multiline
    focus: root.multiline
    readOnly: root.readOnly
    font: root.font
    color: root.color
    wrapMode: TextEdit.WordWrap
    onTextChanged: {
      if (root.text !== edit.text) {
        root.text = edit.text;
      }
    }
    onCursorPositionChanged: {
      root.selectionStart = edit.selectionStart;
      root.selectionEnd = edit.selectionEnd;
      root.selectionChanged();
    }
    Keys.onPressed: {
      root.lastKeyText = root.mapKeyText(event);
      root.keyPressed();
    }
  }

  Text {
    id: placeholder
    anchors.fill: parent
    text: root.placeholderText
    color: root.placeholderTextColor
    font: root.font
    visible: root.text === "" && root.placeholderText !== ""
    z: -1
  }
}
)TEXTINPUT";
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
  parser.addOption(fullscreenOption);
  parser.addOption(bundleOption);
  parser.addOption(screenshotOption);
  parser.process(app);

  const QString bundlePath = parser.isSet(bundleOption)
      ? parser.value(bundleOption)
      : QStringLiteral(RNQNX_PLAYGROUND_BUNDLE_PATH);

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
#ifdef RNQNX_MARATHON_HOST
  window.setTitle(QStringLiteral("React Native MarathonOS playground"));
#else
  window.setTitle(QStringLiteral("react-native-qnx playground"));
#endif
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

  rnqnx::fabric::QtQuickMountingLayer layer(
      factory, *window.contentItem(), kSurfaceId);
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

  rnqnx::runtime::QtReactHost host(adapter);
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
  host.startSurface(
      kSurfaceId,
      "RNQnxPlayground",
      static_cast<float>(window.width()),
      static_cast<float>(window.height()));

  if (parser.isSet(screenshotOption)) {
    for (int i = 0; i < 20; ++i) {
      host.runtime().drainMicrotasks();
      host.pumpEvents();
    }
    const QImage frame = window.grabWindow();
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
}
