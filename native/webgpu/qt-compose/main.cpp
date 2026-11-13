// Stage 5: composes WebGPU output into a Qt Quick scene graph on the device.
//
//   rnqnx_webgpu_qt_compose [--seconds=N] [--out-dir=DIR]
//
// A worker thread renders a WebGPU triangle (Dawn, OpenGL ES) and reads each
// frame back; WebGpuItem uploads it into the Qt Quick scene graph; the window
// is then grabbed from Qt's own render output and checked for the triangle's
// colour. QtGraphicsSurface (stage 1) logs the scene-graph state around it.
//
// This is a CPU-copy composition path (GPU -> readback -> texture upload). It
// is correct and uses only public APIs on both sides; it is not zero-copy.
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
#include <cstring>

#include "Log.h"
#include "QtGraphicsSurface.h"
#include "WebGpuFrameSource.h"
#include "WebGpuItem.h"

using rnqnx::webgpu::logLine;

namespace {
const char* stateName(rnqnx::graphics::GraphicsSurfaceState state) {
  using S = rnqnx::graphics::GraphicsSurfaceState;
  switch (state) {
    case S::Uninitialized: return "Uninitialized";
    case S::Initialized: return "Initialized";
    case S::Invalidated: return "Invalidated";
    case S::Destroyed: return "Destroyed";
    default: return "Closed";
  }
}
}  // namespace

int main(int argc, char** argv) {
  int seconds = 8;
  QString outDir = QStringLiteral(".");
  for (int i = 1; i < argc; ++i) {
    if (!std::strncmp(argv[i], "--seconds=", 10)) seconds = std::atoi(argv[i] + 10);
    else if (!std::strncmp(argv[i], "--out-dir=", 10)) outDir = QString::fromUtf8(argv[i] + 10);
  }

  QGuiApplication app(argc, argv);
  QQuickWindow window;
  window.setTitle(QStringLiteral("rnqnx webgpu qt compose"));
  window.resize(640, 480);
  window.setColor(Qt::black);

  WebGpuItem item(window.contentItem());
  item.setSize(QSizeF(512, 384));
  item.setPosition(QPointF(64, 48));

  rnqnx::graphics::QtGraphicsSurface graphics(window);
  QObject::connect(&graphics, &rnqnx::graphics::QtGraphicsSurface::snapshotChanged, &app,
                   [](rnqnx::graphics::GraphicsSurfaceSnapshot snapshot) {
                     logLine("QTGRAPHICS",
                             "state=%s backend=%d pixel=%dx%d dpr=%.2f generation=%llu visible=%d "
                             "exposed=%d",
                             stateName(snapshot.state), static_cast<int>(snapshot.backend),
                             snapshot.viewport.pixelSize.width(),
                             snapshot.viewport.pixelSize.height(),
                             static_cast<double>(snapshot.viewport.devicePixelRatio),
                             (unsigned long long)snapshot.generation, snapshot.isVisible ? 1 : 0,
                             snapshot.isExposed ? 1 : 0);
                   });
  int swaps = 0;
  QObject::connect(&graphics, &rnqnx::graphics::QtGraphicsSurface::frameSwapped, &app,
                   [&swaps](rnqnx::graphics::GraphicsFrameSwap) { ++swaps; });

  WebGpuFrameSource source(512, 384);
  QObject::connect(
      &source, &WebGpuFrameSource::frameReady, &item,
      [&item, &source](QImage image) {
        item.setFrame(std::move(image));
        source.frameConsumed();
      },
      Qt::QueuedConnection);
  QString error;
  if (!source.start(&error)) {
    logLine("RESULT", "FAIL compose: %s", error.toStdString().c_str());
    return 1;
  }
  window.show();

  int exitCode = 1;
  // Two grabs of the item's corner, a second and a half apart: the clear
  // colour animates, so identical pixels would mean frames are not reaching
  // the scene graph.
  QColor firstCorner;
  QTimer::singleShot((seconds * 1000) / 2, &app, [&]() {
    firstCorner = window.grabWindow().pixelColor(64 + 4, 48 + 4);
  });
  QTimer::singleShot(seconds * 1000, &app, [&]() {
    const QImage grabbed = window.grabWindow();
    // The item is at (64,48) size 512x384; the triangle covers its center.
    const QColor center = grabbed.pixelColor(64 + 256, 48 + 192);
    const QColor itemCorner = grabbed.pixelColor(64 + 4, 48 + 4);
    const QColor corner = grabbed.pixelColor(2, 2);
    const bool centerRed = center.red() > 200 && center.green() < 40 && center.blue() < 40;
    const bool cornerBlack = corner.red() < 8 && corner.green() < 8 && corner.blue() < 8;
    const bool animated = firstCorner.isValid() && qAbs(itemCorner.green() - firstCorner.green()) >= 8;
    logLine("COMPOSE",
            "grab=%dx%d center=(%d,%d,%d) window_corner=(%d,%d,%d) item_corner_first_green=%d "
            "item_corner_last_green=%d frames=%llu frameSwaps=%d",
            grabbed.width(), grabbed.height(), center.red(), center.green(), center.blue(),
            corner.red(), corner.green(), corner.blue(), firstCorner.green(), itemCorner.green(),
            (unsigned long long)source.framesProduced(), swaps);
    grabbed.save(outDir + QStringLiteral("/qt-compose-grab.png"));
    const bool ok = centerRed && cornerBlack && animated && source.framesProduced() > 10;
    logLine("RESULT", "%s compose red_center=%d black_window_corner=%d animated=%d", ok ? "PASS" : "FAIL",
            centerRed, cornerBlack, animated);
    exitCode = ok ? 0 : 1;
    source.stop();
    app.quit();
  });
  app.exec();
  return exitCode;
}
