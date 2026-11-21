// Checks TimingProbe against a real QQuickWindow on the offscreen platform.
// A passing run says the recorder keeps its contract on this host. It says
// nothing about QNX or a display.

#include "TimingProbe.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

using rnqnx::probe::TimingConfig;
using rnqnx::probe::TimingProbe;

#define CHECK(cond)                                                                \
  do {                                                                             \
    if (!(cond)) {                                                                 \
      std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #cond);      \
      std::exit(1);                                                                \
    }                                                                              \
  } while (false)

namespace {

QString configError(const char* a, const char* b, const char* c) {
  QString error;
  const auto parsed = TimingConfig::fromStrings(QString::fromLatin1(a), QString::fromLatin1(b),
                                                QString::fromLatin1(c), &error);
  CHECK(!parsed.has_value());
  return error;
}

void testConfig() {
  QString error;
  const auto defaults = TimingConfig::fromStrings({}, {}, {}, &error);
  CHECK(defaults && defaults->settleMs == 500 && defaults->wheelNotches == 0 &&
        defaults->wheelGapMs == 200 && defaults->frameCapacity == 4096);
  const auto set = TimingConfig::fromStrings(QStringLiteral("0"), QStringLiteral("1000"),
                                             QStringLiteral("5000"), &error);
  CHECK(set && set->settleMs == 0 && set->wheelNotches == 1000 && set->wheelGapMs == 5000);
  CHECK(configError("60001", "", "").contains(QLatin1String("--timing-settle-ms")));
  CHECK(configError("-1", "", "").contains(QLatin1String("--timing-settle-ms")));
  CHECK(configError("", "1001", "").contains(QLatin1String("--timing-wheel-notches")));
  CHECK(configError("", "", "0").contains(QLatin1String("--timing-wheel-gap-ms")));
  CHECK(configError("", "", "5001").contains(QLatin1String("--timing-wheel-gap-ms")));
  CHECK(configError("12x", "", "").contains(QLatin1String("12x")));
  CHECK(configError("1.5", "", "").contains(QLatin1String("1.5")));
}

struct Scene {
  QQuickWindow window;
  QQmlEngine engine;
  QQuickItem* item = nullptr;
  Scene() {
    window.resize(240, 160);
    QQmlComponent component(&engine);
    component.setData(
        "import QtQuick\nRectangle { width: 40; height: 40; color: 'red'\n"
        "  NumberAnimation on x { from: 0; to: 150; duration: 300; loops: Animation.Infinite } }",
        QUrl());
    item = qobject_cast<QQuickItem*>(component.create());
    CHECK(item != nullptr);
    item->setParentItem(window.contentItem());
  }
};

// Runs one probe to completion and returns whether it reported success.
bool run(Scene& scene, TimingProbe& probe, bool startSurfaceAfterFrames) {
  scene.window.show();
  if (startSurfaceAfterFrames) {
    // Let swaps land before the surface starts: they must not become firstFrameMs.
    QEventLoop settle;
    QTimer::singleShot(150, &settle, &QEventLoop::quit);
    settle.exec();
  }
  probe.markBundleEvaluated();
  probe.markSurfaceStarted();
  bool ok = false;
  QEventLoop loop;
  probe.start([&](bool result) {
    ok = result;
    QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
  });
  loop.exec();
  return ok;
}

QJsonObject readJson(const QString& path) {
  QFile file(path);
  CHECK(file.open(QIODevice::ReadOnly));
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  CHECK(doc.isObject());
  return doc.object();
}

void testRecordingAndExport(const QString& dir) {
  QString error;
  auto config = *TimingConfig::fromStrings(QStringLiteral("60"), QStringLiteral("4"),
                                           QStringLiteral("40"), &error);
  Scene scene;
  TimingProbe probe(scene.window, config, std::chrono::steady_clock::now());
  CHECK(run(scene, probe, true));

  const QString path = dir + QStringLiteral("/result.json");
  CHECK(probe.writeJson(path).isEmpty());
  const QJsonObject json = readJson(path);
  CHECK(json.value("schema").toInt() == 1);
  CHECK(json.value("clock").toString() == QLatin1String("steady_clock"));
  CHECK(json.value("platform").toString() == QLatin1String("offscreen"));
  CHECK(json.value("renderBackend").toString() == QLatin1String("software"));
  const QJsonArray frames = json.value("frameMs").toArray();
  CHECK(frames.size() > 2);
  CHECK(json.value("droppedFrames").toInt() == 0);
  CHECK(json.value("totalFrames").toInt() == frames.size());
  double previous = -1;
  bool firstFound = false;
  for (const QJsonValue& value : frames) {
    CHECK(value.toDouble() >= previous);
    previous = value.toDouble();
    if (previous == json.value("firstFrameMs").toDouble()) firstFound = true;
  }
  CHECK(firstFound);
  // Swaps landed before markSurfaceStarted, so the first counted frame is later than frames[0].
  CHECK(json.value("firstFrameMs").toDouble() >= json.value("surfaceStartedMs").toDouble());
  CHECK(json.value("firstFrameMs").toDouble() > frames.first().toDouble());
  CHECK(json.value("bundleEvaluatedMs").toDouble() <= json.value("surfaceStartedMs").toDouble());
  CHECK(json.value("wheelNotchesSent").toInt() == 4);
  CHECK(json.value("wheelLastMs").toDouble() - json.value("wheelFirstMs").toDouble() >= 3 * 40 - 5);
  CHECK(json.value("rssSource").toString() == QLatin1String("os"));
  CHECK(json.value("rssKiB").toDouble() > 0);
  CHECK(json.value("peakRssKiB").toDouble() >= json.value("rssKiB").toDouble());

  // No overwrite: the second export fails and leaves the first file as it was.
  QFile before(path);
  CHECK(before.open(QIODevice::ReadOnly));
  const QByteArray bytes = before.readAll();
  before.close();
  CHECK(!probe.writeJson(path).isEmpty());
  QFile after(path);
  CHECK(after.open(QIODevice::ReadOnly));
  CHECK(after.readAll() == bytes);
  // And no temporary file is left behind.
  CHECK(QDir(dir).entryList(QDir::Files | QDir::Hidden).size() == 1);
}

void testOverflowIsCounted() {
  QString error;
  auto config = *TimingConfig::fromStrings(QStringLiteral("0"), QStringLiteral("5"),
                                           QStringLiteral("60"), &error);
  config.frameCapacity = 4;
  Scene scene;
  TimingProbe probe(scene.window, config, std::chrono::steady_clock::now());
  CHECK(run(scene, probe, false));
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("overflow.json"));
  CHECK(probe.writeJson(path).isEmpty());
  const QJsonObject json = readJson(path);
  CHECK(json.value("frameMs").toArray().size() == 4);
  CHECK(json.value("totalFrames").toInt() > 4);
  CHECK(json.value("droppedFrames").toInt() == json.value("totalFrames").toInt() - 4);
}

void testProbeOutlivesNothing() {
  // Destroying the probe while the window keeps swapping must not touch freed state.
  Scene scene;
  {
    TimingProbe probe(scene.window, TimingConfig{}, std::chrono::steady_clock::now());
    scene.window.show();
    probe.markSurfaceStarted();
  }
  QEventLoop loop;
  QTimer::singleShot(200, &loop, &QEventLoop::quit);
  loop.exec();
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  testConfig();
  QTemporaryDir dir;
  CHECK(dir.isValid());
  testRecordingAndExport(dir.path());
  testOverflowIsCounted();
  testProbeOutlivesNothing();
  std::puts("RNQNX_TIMING_PROBE_TEST_OK config recording export overflow lifetime");
  return 0;
}
