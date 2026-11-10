#include "QtWorkletsScheduler.h"
#include <QCoreApplication>
#include <QThread>
#include <memory>
#include <stdexcept>
#include <thread>
#include <iostream>
using rnqnx::runtime::QtWorkletsScheduler;
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  int calls = 0;
  int errors = 0;
  auto scheduler = std::make_shared<QtWorkletsScheduler>([&](std::string message) {
    if (message == "expected") ++errors;
  });
  std::thread worker([&] {
    scheduler->scheduleOnUI([&] {
      if (QThread::currentThread() != app.thread()) throw std::runtime_error("wrong owner");
      ++calls;
    });
    scheduler->scheduleOnUI([] { throw std::runtime_error("expected"); });
  });
  worker.join();
  QCoreApplication::processEvents();
  if (calls != 1 || errors != 1) return 1;
  scheduler->scheduleOnUI([&] { ++calls; });
  scheduler->close();
  std::thread late([&] { scheduler->scheduleOnUI([&] { ++calls; }); });
  late.join();
  QCoreApplication::processEvents();
  // Once closed on the owner, remaining references may be dropped elsewhere.
  std::thread release([scheduler = std::move(scheduler)]() mutable { scheduler.reset(); });
  release.join();
  if (calls != 1) return 2;
  std::cout << "RNQNX_WORKLETS_SCHEDULER_OK owner error close late-admission\n";
}
