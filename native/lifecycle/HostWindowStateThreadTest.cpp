// Its own process: the singleton must be first reached from a thread that is not
// the QGuiApplication owner, which no test sharing a process with other tests
// can promise. This is the order AppState sees when JavaScript has its own thread.

#include "HostWindowState.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QThread>
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

using rnqnx::runtime::HostWindowState;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
  if (ok) return;
  std::fprintf(stderr, "FAIL: %s\n", what);
  ++failures;
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);

  std::atomic<QThread*> instanceThread{nullptr};
  std::thread worker([&] { instanceThread = HostWindowState::instance().thread(); });
  worker.join();
  auto& host = HostWindowState::instance();
  check(instanceThread.load() == application.thread(), "first call on a worker lives on the owner thread");
  check(host.thread() == application.thread(), "instance lives on the owner thread");

  bool attachRejected = false;
  std::thread foreign([&] {
    try { host.attach(application); }
    catch (const std::logic_error&) { attachRejected = true; }
  });
  foreign.join();
  check(attachRejected, "attach from a foreign thread is rejected");

  host.attach(application);
  host.attach(application);  // idempotent

  // The mirror follows Qt even though a worker created the object.
  QMetaObject::invokeMethod(&application, "applicationStateChanged", Qt::DirectConnection,
      Q_ARG(Qt::ApplicationState, Qt::ApplicationHidden));
  check(host.qtApplicationState() == Qt::ApplicationHidden, "mirror updated by a direct signal");

  std::vector<bool> seen;
  QObject::connect(&host, &HostWindowState::minimizedChanged, &host,
      [&seen](bool minimized) { seen.push_back(minimized); });
  std::thread setter([&] {
    host.setMinimized(true);
    host.setMinimized(true);  // no change, no second signal
    host.setMinimized(false);
  });
  setter.join();
  check(host.minimized() == false, "flag is atomic and visible at once");
  check(seen.empty(), "foreign-thread emission waits for the owner event loop");
  QCoreApplication::processEvents();
  check(seen.size() == 2 && seen[0] == true && seen[1] == false, "owner thread receives true then false once each");

  host.setMinimized(true);  // owner thread: synchronous
  check(seen.size() == 3 && seen[2] == true, "owner-thread change emits synchronously");
  host.setMinimized(false);

  if (failures == 0) std::puts("host window state thread test: ok");
  return failures == 0 ? 0 : 1;
}
