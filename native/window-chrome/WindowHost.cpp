#include "WindowHost.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>
#include <QWindow>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#if defined(Q_OS_QNX)
#include <screen/screen.h>
#include <unistd.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace rnqnx::window {
namespace {

#if defined(Q_OS_QNX)
// False when the window manager shows another window in front of this one.
bool nativeWindowVisible(QWindow& window) {
  if (window.handle() == nullptr) return false;
  auto native = reinterpret_cast<screen_window_t>(window.winId());
  int visible = 1;
  if (native == nullptr ||
      screen_get_window_property_iv(native, SCREEN_PROPERTY_VISIBLE, &visible) != 0)
    return true;
  return visible != 0;
}
#endif

#if defined(Q_OS_QNX)
constexpr char kManagerHello[] = "rnqnx:hello";
constexpr char kManagerReply[] = "rnqnx-winmgr";

screen_window_t nativeHandle(QWindow& window) {
  if (window.handle() == nullptr) return nullptr;
  return reinterpret_cast<screen_window_t>(window.winId());
}

void flushWindow(screen_window_t native) {
  screen_context_t context = nullptr;
  if (screen_get_window_property_pv(native, SCREEN_PROPERTY_CONTEXT,
                                    reinterpret_cast<void**>(&context)) == 0 &&
      context != nullptr)
    screen_flush_context(context, 0);
}

// Sets the window's manager string and pushes it to Screen straight away.
bool setManagerString(screen_window_t native, const char* text) {
  if (screen_set_window_property_cv(native, SCREEN_PROPERTY_MANAGER_STRING,
                                    static_cast<int>(std::strlen(text)), text) != 0)
    return false;
  flushWindow(native);
  return true;
}
#endif

QString currentUser() {
#if defined(Q_OS_UNIX)
  return QString::number(static_cast<qulonglong>(::getuid()));
#else
  return qEnvironmentVariable("USERNAME", QStringLiteral("user"));
#endif
}

}  // namespace

bool windowExpanded(const QWindow& window) {
  if (window.windowStates() & (Qt::WindowFullScreen | Qt::WindowMaximized)) return true;
  const QScreen* screen = window.screen();
  if (screen == nullptr) return false;
  const QSize available = screen->availableGeometry().size();
  return available.isValid() && window.width() >= available.width() &&
         window.height() >= available.height();
}

bool windowManagerKeepsWindowSize([[maybe_unused]] QWindow& window, [[maybe_unused]] int timeoutMs) {
#if defined(Q_OS_QNX)
  const QByteArray forced = qgetenv("RNQNX_WINDOW_MANAGER");
  if (forced == "windowed") return true;
  if (forced == "fullscreen") return false;
  window.create();
  screen_window_t native = nativeHandle(window);
  if (native == nullptr || !setManagerString(native, kManagerHello)) return false;
  // The property holds one value: ours until the manager writes its answer.
  // fullscreen-winmgr answers with its own name; a missing answer means no
  // manager or one that ignores the property.
  QElapsedTimer elapsed;
  elapsed.start();
  while (elapsed.elapsed() < timeoutMs) {
    char answer[256] = {};
    if (screen_get_window_property_cv(native, SCREEN_PROPERTY_MANAGER_STRING, sizeof answer - 1,
                                      answer) == 0 &&
        answer[0] != '\0' && std::strcmp(answer, kManagerHello) != 0) {
      return std::strncmp(answer, kManagerReply, sizeof kManagerReply - 1) == 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#endif
  return false;
}

void applyNativePosition([[maybe_unused]] QWindow& window) {
#if defined(Q_OS_QNX)
  screen_window_t native = nativeHandle(window);
  if (native == nullptr) return;
  int position[2] = {window.x(), window.y()};
  screen_set_window_property_iv(native, SCREEN_PROPERTY_POSITION, position);
  flushWindow(native);
#endif
}

void sendWindowManagerRequest([[maybe_unused]] QWindow& window, [[maybe_unused]] const char* request) {
#if defined(Q_OS_QNX)
  // The launcher talks to the manager the same way: a SCREEN_EVENT_MANAGER
  // event naming the window, injected on its display, with `key=value` text.
  screen_window_t native = nativeHandle(window);
  if (native == nullptr) return;
  screen_display_t display = nullptr;
  screen_event_t event = nullptr;
  if (screen_get_window_property_pv(native, SCREEN_PROPERTY_DISPLAY,
                                    reinterpret_cast<void**>(&display)) != 0 ||
      display == nullptr || screen_create_event(&event) != 0)
    return;
  const QByteArray text = QByteArrayLiteral("rnqnx=") + request;
  const int type = SCREEN_EVENT_MANAGER;
  if (screen_set_event_property_iv(event, SCREEN_PROPERTY_TYPE, &type) != 0 ||
      screen_set_event_property_pv(event, SCREEN_PROPERTY_WINDOW, reinterpret_cast<void**>(&native)) != 0 ||
      screen_set_event_property_cv(event, SCREEN_PROPERTY_USER_DATA, static_cast<int>(text.size()),
                                   text.constData()) != 0 ||
      screen_inject_event(display, event) != 0) {
    std::fprintf(stderr, "RNQNX_WINDOW_MANAGER_WARNING cannot send %s\n", text.constData());
  }
  screen_destroy_event(event);
  flushWindow(native);
#endif
}

SingleInstance::SingleInstance(const QString& appId, QObject* parent)
    : QObject(parent),
      serverName_(QStringLiteral("rnqnx-%1-%2").arg(appId, currentUser())) {}

SingleInstance::~SingleInstance() {
  if (server_) server_->close();
}

bool SingleInstance::forwardedToRunningInstance() {
  QLocalSocket socket;
  socket.connectToServer(serverName_);
  if (!socket.waitForConnected(500)) return false;
  socket.write("restore\n");
  if (!socket.waitForBytesWritten(500)) return false;
  // Wait for the acknowledgement so the request is not lost when this process
  // exits straight away.
  socket.waitForReadyRead(1000);
  socket.disconnectFromServer();
  return true;
}

bool SingleInstance::listen() {
  auto* server = new QLocalServer(this);
  server->setSocketOptions(QLocalServer::UserAccessOption);
  if (!server->listen(serverName_)) {
    // Remove the socket file only when it is stale, left by a process that
    // crashed. A live instance that was too busy to answer within the timeout
    // in forwardedToRunningInstance() still owns it, and unlinking its socket
    // would leave that instance unreachable.
    QLocalSocket probe;
    probe.connectToServer(serverName_);
    const bool stale = !probe.waitForConnected(200) &&
                       (probe.error() == QLocalSocket::ServerNotFoundError ||
                        probe.error() == QLocalSocket::ConnectionRefusedError);
    probe.abort();
    if (stale) QLocalServer::removeServer(serverName_);
    if (!stale || !server->listen(serverName_)) {
      std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE_WARNING cannot listen on %s: %s\n",
                   serverName_.toUtf8().constData(), server->errorString().toUtf8().constData());
      delete server;
      return false;
    }
  }
  server_ = server;
  connect(server, &QLocalServer::newConnection, this, &SingleInstance::acceptConnections);
  return true;
}

void SingleInstance::acceptConnections() {
  while (server_ && server_->hasPendingConnections()) {
    QLocalSocket* socket = server_->nextPendingConnection();
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
      while (socket->canReadLine()) {
        if (socket->readLine().trimmed() == QByteArrayLiteral("restore")) {
          socket->write("ok\n");
          socket->flush();
          Q_EMIT restoreRequested();
        }
      }
    });
  }
}

void minimizeByDestroyingWindow(QWindow& window) {
  window.hide();
  // Releasing the native window is what makes fullscreen-winmgr show the
  // launcher again; the QQuickWindow, its items and the React tree stay.
  window.destroy();
}

void restoreMinimizedWindow(QWindow& window, bool sizeKeepingManager) {
  if (sizeKeepingManager && window.isVisible()) {
    // rnqnx-winmgr hid the window and keeps it; it shows and raises it.
    sendWindowManagerRequest(window, "restore");
  } else if (!window.isVisible()) {
    window.show();
  }
#if defined(Q_OS_QNX)
  else if (!sizeKeepingManager && !nativeWindowVisible(window)) {
    // fullscreen-winmgr shows the most recent window, and its switch message
    // needs a tag this window may not have, so make it the most recent one.
    window.hide();
    window.destroy();
    window.show();
  }
#endif
  window.raise();
  window.requestActivate();
}

}  // namespace rnqnx::window
