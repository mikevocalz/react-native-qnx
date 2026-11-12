#include "WindowHost.h"

#include <QByteArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>
#include <QWindow>

#include <cstdio>

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

void restoreMinimizedWindow(QWindow& window) {
  if (!window.isVisible()) {
    window.show();
  }
#if defined(Q_OS_QNX)
  else if (!nativeWindowVisible(window)) {
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
