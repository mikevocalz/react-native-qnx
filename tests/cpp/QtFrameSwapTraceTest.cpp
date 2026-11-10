#include "react-native-qnx/runtime/QtFrameSwapTrace.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace rnqnx::runtime;
namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void signalLifecycle() {
  QQuickWindow window;
  auto trace = std::make_shared<RuntimeTrace>(8);
  {
    QtFrameSwapTrace observer(window, trace);
    // Deterministic signal injection tests the connection, not rendering.
    require(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection),
        "signal injection failed");
    bool workerInvoked = false;
    std::thread worker([&] {
      workerInvoked = QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection);
    });
    worker.join();
    require(workerInvoked, "render-thread signal injection failed");
  }
  require(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection),
      "post-disconnect signal injection failed");
  const auto snapshot = trace->freeze();
  require(snapshot.events.size() == 2 && snapshot.dropped == 0, "disconnect or record count");
  const auto& first = snapshot.events[0];
  const auto& second = snapshot.events[1];
  require(first.stage == TraceStage::FrameSwapped && second.stage == TraceStage::FrameSwapped &&
      first.phase == TracePhase::Instant && second.phase == TracePhase::Instant, "signal stage/phase");
  require(first.sequence == 1 && second.sequence == 2 &&
      first.recordedAtNs <= second.recordedAtNs && first.correlation != second.correlation,
      "signal sequence/timestamp/correlation");
  require(first.thread != second.thread, "callback must record on emitting thread without GUI hop");
  require(first.surface == 0 && second.surface == 0 &&
      first.surfaceGeneration == 0 && second.surfaceGeneration == 0,
      "window signal must not imply a surface frame");
  std::ostringstream encoded;
  RuntimeTrace::writeJson(snapshot, encoded);
  require(encoded.str().find("\"stage\":\"qt.frame-swapped\"") != std::string::npos,
      "signal trace export");

  auto frozen = std::make_shared<RuntimeTrace>(2);
  QtFrameSwapTrace afterFreeze(window, frozen);
  frozen->freeze();
  require(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection), "frozen injection");
  require(frozen->freeze().events.empty(), "frozen session accepted late signal");
  QtFrameSwapTrace disabled(window, nullptr);
}
void senderLifetime() {
  auto window = std::make_unique<QQuickWindow>();
  auto trace = std::make_shared<RuntimeTrace>(2);
  std::weak_ptr<RuntimeTrace> weak = trace;
  auto observer = std::make_unique<QtFrameSwapTrace>(*window, trace);
  trace.reset();
  require(!weak.expired(), "connection must retain its callback session");
  window.reset();
  require(weak.expired(), "destroyed sender retained callback session");
  observer.reset(); // Disconnecting after sender destruction is safe.
}
void actualWindowSignal() {
  auto trace = std::make_shared<RuntimeTrace>(64);
  bool timedOut = false;
  {
    QQuickWindow window;
    window.resize(64, 64);
    window.setColor(Qt::blue);
    QtFrameSwapTrace observer(window, trace);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    // Quit through the GUI event loop; do not access GUI state in a render callback.
    QObject::connect(&window, &QQuickWindow::frameSwapped, &loop, &QEventLoop::quit,
        Qt::QueuedConnection);
    timeout.start(5000);
    window.show();
    window.requestUpdate();
    loop.exec();
    window.hide();
  } // Join the window/render lifetime before freezing/exporting the session.
  const auto snapshot = trace->freeze();
  require(!timedOut && !snapshot.events.empty(), "real offscreen Qt frameSwapped was not observed");
  require(snapshot.events[0].stage == TraceStage::FrameSwapped, "real window trace stage");
  require(snapshot.dropped == 0, "unexpected trace truncation");
}
} // namespace
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    signalLifecycle();
    senderLifetime();
    actualWindowSignal();
    std::cout << "RNQNX_QT_FRAME_SWAP_TRACE_OK signal lifetime sequence export offscreen-window\n";
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_QT_FRAME_SWAP_TRACE_FAIL " << error.what() << '\n';
    return 1;
  }
}
