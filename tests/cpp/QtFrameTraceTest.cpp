#include "react-native-qnx/runtime/QtFrameTrace.h"

#include <QGuiApplication>
#include <QQuickWindow>

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace rnqnx::runtime;

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void signal(QQuickWindow& window) {
  require(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection),
      "Qt frame signal must be invokable");
}

void recordingAndDisconnect() {
  QQuickWindow window;
  const auto trace = std::make_shared<RuntimeTrace>(8);
  {
    QtFrameTrace observer(window, trace, 29, 6);
    signal(window);
    signal(window);
  }
  signal(window);
  const auto snapshot = trace->freeze();
  require(snapshot.events.size() == 2 && snapshot.dropped == 0, "observer disconnect");
  for (const auto& event : snapshot.events) {
    require(event.stage == TraceStage::FrameQueued && event.phase == TracePhase::Instant,
        "frame boundary must be queued, not presented");
    require(event.surface == 29 && event.surfaceGeneration == 6, "immutable trace scope");
    require(event.correlation != 0, "frame correlation");
  }
  require(snapshot.events[0].correlation != snapshot.events[1].correlation,
      "distinct frame correlations");
  std::ostringstream output;
  RuntimeTrace::writeJson(snapshot, output);
  require(output.str().find("\"stage\":\"qt.frame.queued\"") != std::string::npos,
      "serialized queued boundary");
  require(output.str().find("not display presentation") != std::string::npos,
      "serialized capture limitation");
}

void boundedAndFrozen() {
  QQuickWindow window;
  const auto trace = std::make_shared<RuntimeTrace>(1);
  QtFrameTrace observer(window, trace);
  signal(window);
  signal(window);
  const auto snapshot = trace->freeze();
  require(snapshot.events.size() == 1 && snapshot.dropped == 1, "bounded frame capture");
  signal(window);
  const auto frozen = trace->freeze();
  require(frozen.events.size() == 1 && frozen.dropped == 1, "freeze closes frame recording");
}

void independentLifetimes() {
  auto window = std::make_unique<QQuickWindow>();
  auto trace = std::make_shared<RuntimeTrace>(4);
  auto observer = std::make_unique<QtFrameTrace>(*window, trace);
  trace.reset();
  signal(*window);
  window.reset();
  observer.reset();
  QQuickWindow secondWindow;
  QtFrameTrace disabled(secondWindow, nullptr);
  signal(secondWindow);
}

void ownerThread() {
  QQuickWindow window;
  const auto trace = std::make_shared<RuntimeTrace>(4);
  bool rejected = false;
  std::thread worker([&] {
    try {
      QtFrameTrace observer(window, trace);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
  });
  worker.join();
  require(rejected && trace->freeze().events.empty(), "wrong-owner attachment rejected");
}
} // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    recordingAndDisconnect();
    boundedAndFrozen();
    independentLifetimes();
    ownerThread();
    std::cout << "RNQNX_QT_FRAME_TRACE_OK signal/lifetime contract only; no rendered-frame or presentation evidence\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_QT_FRAME_TRACE_FAIL " << error.what() << '\n';
    return 1;
  }
}
