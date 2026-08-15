// Runs apps/rn-playground in a real window.
//
// The host tests prove the same path under the offscreen platform plugin; this
// exists so a person can look at the result. It is also the shape the QNX
// runner will take once a target exists: create a window, build a host, load a
// bundle, start a surface, run the event loop.
//
// Usage:
//   rnqnx_playground_host [--bundle <path>] [--screenshot <path>]
//
// With --screenshot it renders, writes a PNG, and exits, which is how it stays
// useful on a machine with no display.

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

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
#include <QString>
#include <QTimer>

#include <cstdio>

namespace {

constexpr facebook::react::SurfaceId kSurfaceId = 1;
constexpr const char* kViewSource = "import QtQuick\nRectangle { visible: true }\n";
constexpr const char* kTextSource = "import QtQuick\nText { visible: true }\n";
constexpr const char* kImageSource = "import QtQuick\nImage { visible: true }\n";
constexpr const char* kScrollViewSource =
    "import QtQuick\nFlickable { visible: true; clip: true }\n";
constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 480;

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("rnqnx-playground"));

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
  window.setTitle(QStringLiteral("react-native-qnx playground"));
  window.setWidth(kWindowWidth);
  window.setHeight(kWindowHeight);
  window.setColor(QColor(Qt::white));

  QQmlEngine engine;
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(kViewSource));
  factory.registerType(QStringLiteral("Text"), QByteArray(kTextSource));
  factory.registerType(QStringLiteral("Image"), QByteArray(kImageSource));
  factory.registerType(
      QStringLiteral("ScrollView"), QByteArray(kScrollViewSource));

  rnqnx::fabric::QtQuickMountingLayer layer(
      factory, *window.contentItem(), kSurfaceId);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  adapter.mapComponent(QStringLiteral("Paragraph"), QStringLiteral("Text"));
  adapter.mapComponent(QStringLiteral("Image"), QStringLiteral("Image"));
  adapter.mapComponent(
      QStringLiteral("ScrollView"), QStringLiteral("ScrollView"));

  rnqnx::runtime::QtReactHost host(adapter);
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

  window.show();
  host.startSurface(
      kSurfaceId,
      "RNQnxPlayground",
      static_cast<float>(kWindowWidth),
      static_cast<float>(kWindowHeight));

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
