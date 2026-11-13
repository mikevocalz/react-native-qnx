// The react-native-svg fixture corpus through the production path: Metro,
// Hermes, react-native-svg 15.16.0's SvgXml, Fabric, the Qt mounting layer and
// the real window. Each batch of cells (apps/rn-playground/rnsvg-corpus.js) is
// grabbed from the window, every cell is cropped out and compared with its resvg
// golden by the same diff and thresholds the host runner uses
// (tests/native/qt-rnsvg/RnsvgFixturesTest.cpp), read from the same manifest.
//
// Pending fixtures are not compared. Set RNQNX_RNSVG_FAIL_ON_PENDING=1 to make
// a pending fixture a failure, as in the host runner.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <RnsvgItems.h>

#include "../qt-rnsvg/support/FixtureManifest.h"
#include "../qt-rnsvg/support/ImageDiff.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQmlEngine>
#include <QQuickWindow>

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace rnqnx::views::rnsvg::testing;

namespace {

void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void collectRoots(QQuickItem* parent, std::vector<rnqnx::views::RnsvgRootItem*>& out) {
  if (auto* item = qobject_cast<rnqnx::views::RnsvgRootItem*>(parent)) out.push_back(item);
  for (auto* child : parent->childItems()) collectRoots(child, out);
}

// True while some Image element is still decoding its source (ImageState::Loading is 1).
bool anyImageLoading(const QQuickItem* parent) {
  if (const auto* e = qobject_cast<const rnqnx::views::RnsvgElementItem*>(parent)) {
    if (e->kind() == rnqnx::views::rnsvg::Kind::Image && e->props().imageState == 1) return true;
  }
  for (const auto* child : parent->childItems()) {
    if (anyImageLoading(child)) return true;
  }
  return false;
}

struct Cell {
  QString name;
  QRect rect;
};

void await(rnqnx::runtime::QtReactHost& host, const std::function<bool()>& ready, const std::string& what) {
  QElapsedTimer timer;
  timer.start();
  do {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    if (ready()) return;
  } while (timer.elapsed() < 20000);
  std::string message = "Timed out waiting for: " + what;
  for (const auto& error : host.javaScriptErrors()) message += "\n  JS error: " + error.message + "\n" + error.stack;
  for (const auto& failure : host.delegate().lastFailures()) message += "\n  mount failure: " + failure.detail.toStdString();
  throw std::runtime_error(message);
}

// The window pixels of `rect` over white, at 1x.
QImage cropCell(const QImage& window, const QRect& rect) {
  QImage out(rect.size(), QImage::Format_RGB32);
  out.fill(Qt::white);
  QPainter painter(&out);
  painter.drawImage(QPoint(0, 0), window, rect);
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  const bool failOnPending = qEnvironmentVariableIntValue("RNQNX_RNSVG_FAIL_ON_PENDING") != 0;
  // The goldens are resvg renders with the bundled DejaVu faces, so the test must not depend on host fonts.
  for (const char* font : {"DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSansMono.ttf", "DejaVuSerif.ttf"}) {
    const int id = QFontDatabase::addApplicationFont(QStringLiteral(RNQNX_RNSVG_FIXTURE_DIR "/../../native/qt-rnsvg/fonts/") + QLatin1String(font));
    if (id < 0) {
      std::cerr << "rnsvg corpus test error: cannot load font " << font << "\n";
      return 2;
    }
  }
  QFont base(QStringLiteral("DejaVu Sans"));
  base.setPixelSize(12);
  QGuiApplication::setFont(base);
  int passed = 0, pending = 0, failed = 0;
  try {
    QString error;
    const QString fixtureDir = QDir(QStringLiteral(RNQNX_RNSVG_FIXTURE_DIR)).canonicalPath();
    std::map<QString, Fixture> fixtures;
    for (auto& f : loadFixtureManifest(fixtureDir, &error)) fixtures.emplace(f.name, f);
    check(error.isEmpty() && !fixtures.empty(), "manifest.json could not be read");

    const QJsonDocument layout = QJsonDocument::fromJson(readFileBytes(QStringLiteral(RNQNX_RNSVG_CORPUS_LAYOUT)));
    check(layout.isObject(), "corpus layout is missing");
    const QJsonArray size = layout.object().value(QStringLiteral("window")).toArray();
    const int windowW = size.at(0).toInt(), windowH = size.at(1).toInt();
    check(windowW > 0 && windowH > 0, "corpus layout has no window size");

    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(windowW, windowH);
    QQmlEngine engine;
    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("View"), "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n");
    rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    rnqnx::fabric::QtTouchDispatcher dispatcher(layer, *window.contentItem());
    adapter.setTouchDispatcher(&dispatcher);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    rnqnx::runtime::QtReactHost host(adapter);
    host.delegate().setTouchDispatcher(&dispatcher);
    host.installTurboModuleBinding();
    host.installComponentProvider();
    host.installPlatformConstants();
    host.installTimers();

    std::ifstream file(RNQNX_RNSVG_CORPUS_BUNDLE_PATH);
    check(file.good(), "Metro corpus bundle is missing");
    const std::string bundle((std::istreambuf_iterator<char>(file)), {});
    if (!host.evaluate(bundle, "qt-rnsvg-corpus-bundle.js")) throw std::runtime_error(host.lastError().toStdString());
    host.startSurface(11, "RnqnxRnsvgCorpus", float(windowW), float(windowH));
    window.show();

    auto& runtime = host.runtime();
    const int batchCount = int(runtime.global().getProperty(runtime, "__rnsvgCorpusBatchCount").asNumber());
    check(batchCount == layout.object().value(QStringLiteral("batches")).toArray().size(), "bundle and layout disagree on the batch count");

    for (int b = 0; b < batchCount; ++b) {
      QElapsedTimer batchTimer;  // TEMP-PROGRESS
      batchTimer.start();  // TEMP-PROGRESS
      std::cerr << "TEMP batch " << b << " start\n";  // TEMP-PROGRESS
      std::vector<Cell> cells;
      for (const QJsonValue& v : layout.object().value(QStringLiteral("batches")).toArray().at(b).toArray()) {
        const QJsonObject o = v.toObject();
        cells.push_back({o.value(QStringLiteral("name")).toString(),
                         QRect(o.value(QStringLiteral("x")).toInt(), o.value(QStringLiteral("y")).toInt(),
                               o.value(QStringLiteral("w")).toInt(), o.value(QStringLiteral("h")).toInt())});
      }
      if (!host.evaluate("__rnsvgCorpusSetBatch(" + std::to_string(b) + ");", "qt-rnsvg-corpus-batch.js")) {
        throw std::runtime_error(host.lastError().toStdString());
      }
      // Every cell mounts one <Svg>; the previous batch's roots are unmounted by the same commit.
      await(host, [&] {
        std::vector<rnqnx::views::RnsvgRootItem*> roots;
        collectRoots(window.contentItem(), roots);
        if (roots.size() != cells.size()) return false;
        for (auto* r : roots) {
          if (r->paintCount() < 1) return false;
        }
        return !anyImageLoading(window.contentItem());
      }, "batch " + std::to_string(b) + " to mount");
      check(host.javaScriptErrors().empty(), "React reported a JavaScript error");
      check(host.delegate().lastFailures().empty(), "Fabric mount reported a failure");
      // Let the painted textures reach the window.
      for (int i = 0; i < 3; ++i) {
        host.pumpEvents();
        host.runtime().drainMicrotasks();
      }
      std::cerr << "TEMP batch " << b << " mounted " << batchTimer.elapsed() << " ms\n";  // TEMP-PROGRESS
      const QImage grabbed = window.grabWindow();
      std::cerr << "TEMP batch " << b << " grabbed " << batchTimer.elapsed() << " ms\n";  // TEMP-PROGRESS
      check(!grabbed.isNull(), "window grab failed");

      for (const Cell& cell : cells) {
        const auto it = fixtures.find(cell.name);
        check(it != fixtures.end(), "layout names a fixture the manifest does not have");
        const Fixture& fx = it->second;
        if (fx.pending()) {
          ++pending;
          if (failOnPending) {
            ++failed;
            std::cerr << "FAIL " << fx.name.toStdString() << ": pending: " << fx.pendingFeature().toStdString() << "\n";
          }
          continue;
        }
        QImage golden;
        check(golden.load(fx.goldenPath), "golden image is missing");
        golden = golden.convertToFormat(QImage::Format_RGB32);
        const QImage actual = cropCell(grabbed, cell.rect);
        const DiffMetrics m = compareImages(actual, golden, fx.thresholds.channelThreshold);
        const QString why = checkThresholds(m, fx.thresholds);
        if (why.isEmpty()) {
          ++passed;
          continue;
        }
        ++failed;
        actual.save(fx.name + QStringLiteral(".actual.png"));
        const QImage diff = diffImage(actual, golden, fx.thresholds.channelThreshold);
        if (!diff.isNull()) diff.save(fx.name + QStringLiteral(".diff.png"));
        std::cerr << "FAIL " << fx.name.toStdString() << ": " << why.toStdString() << " (wrote " << fx.name.toStdString()
                  << ".actual.png and .diff.png)\n";
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "rnsvg corpus test error: " << e.what() << "\n";
    return 2;
  }
  std::cout << "rnsvg-corpus: " << passed << " passed, " << pending << " pending, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
