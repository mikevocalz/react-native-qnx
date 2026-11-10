#include "QtWorkletsFrameSource.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QThread>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace rnqnx::runtime;
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  int errors = 0;
  auto scheduler = std::make_shared<QtWorkletsScheduler>([&](std::string) { ++errors; });
  auto source = std::make_shared<QtWorkletsFrameSource>(scheduler);
  try {
    bool rejected = false;
    try { source->request([](double) {}); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Missing window silently accepted");
    auto window = std::make_unique<QQuickWindow>();
    window->resize(64, 64);
    source->attach(window.get());
    int frames = 0;
    source->request([&](double timestamp) {
      if (timestamp <= 0 || QThread::currentThread() != app.thread()) throw std::runtime_error("Invalid frame callback");
      ++frames;
    });
    window->show();
    QElapsedTimer deadline;
    deadline.start();
    while (!frames && deadline.elapsed() < 5000) QCoreApplication::processEvents();
    if (frames != 1 || errors != 0) throw std::runtime_error("Actual frame not delivered once on owner");
    source->request([&](double) { ++frames; });
    bool injected = false;
    // Deterministic render-thread signal injection is teardown coverage only;
    // the earlier callback above came from a real offscreen window frame.
    std::thread render([&] {
      injected = QMetaObject::invokeMethod(window.get(), "frameSwapped", Qt::DirectConnection);
    });
    render.join();
    if (!injected) throw std::runtime_error("Could not inject queued render signal");
    window.reset();
    QCoreApplication::processEvents();
    if (frames != 1 || source->ready()) throw std::runtime_error("Destroyed window delivered pending callback");
    rejected = false;
    try { source->request([](double) {}); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Destroyed frame source accepted work");
    source->close();
    // Closing from a callback must cancel the remainder of the local snapshot.
    QQuickWindow secondWindow;
    secondWindow.resize(64, 64);
    auto secondSource = std::make_shared<QtWorkletsFrameSource>(scheduler);
    secondSource->attach(&secondWindow);
    int closedDuringDelivery = 0;
    int lateDelivery = 0;
    secondSource->request([&](double) { ++closedDuringDelivery; secondSource->close(); });
    secondSource->request([&](double) { ++lateDelivery; });
    secondWindow.show();
    deadline.restart();
    while (!closedDuringDelivery && deadline.elapsed() < 5000) QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    if (closedDuringDelivery != 1 || lateDelivery != 0) throw std::runtime_error("Frame snapshot delivered after close");
    scheduler->close();
    std::cout << "RNQNX_WORKLETS_FRAMES_OK real-frame owner destroyed-window cancellation\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
