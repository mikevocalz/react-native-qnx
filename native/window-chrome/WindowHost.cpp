#include "WindowHost.h"

#include <QByteArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <memory>

#if defined(Q_OS_QNX)
#include <screen/screen.h>
#include <unistd.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace rnqnx::window {
namespace {

#if defined(Q_OS_QNX)
// One pass: when Screen reports a different size than Qt asked for, put Qt's
// size back. Returns true when a correction was written.
bool correctNativeGeometry(QWindow& window) {
  if (window.handle() == nullptr || !window.isVisible()) return false;
  if (window.windowStates() & (Qt::WindowFullScreen | Qt::WindowMaximized)) return false;
  auto native = reinterpret_cast<screen_window_t>(window.winId());
  if (native == nullptr) return false;
  const qreal ratio = window.devicePixelRatio();
  const int wanted[2] = {static_cast<int>(window.width() * ratio),
                         static_cast<int>(window.height() * ratio)};
  int actual[2] = {0, 0};
  if (screen_get_window_property_iv(native, SCREEN_PROPERTY_SIZE, actual) != 0) return false;
  if (actual[0] == wanted[0] && actual[1] == wanted[1]) return false;
  const int position[2] = {static_cast<int>(window.x() * ratio), static_cast<int>(window.y() * ratio)};
  screen_set_window_property_iv(native, SCREEN_PROPERTY_SIZE, wanted);
  screen_set_window_property_iv(native, SCREEN_PROPERTY_SOURCE_SIZE, wanted);
  screen_set_window_property_iv(native, SCREEN_PROPERTY_POSITION, position);
  screen_context_t context = nullptr;
  if (screen_get_window_property_pv(native, SCREEN_PROPERTY_CONTEXT,
                                    reinterpret_cast<void**>(&context)) == 0 &&
      context != nullptr) {
    screen_flush_context(context, 0);
  }
  std::printf("RNQNX_WINDOW_GEOMETRY restored=%dx%d+%d+%d was=%dx%d\n", wanted[0], wanted[1],
              position[0], position[1], actual[0], actual[1]);
  std::fflush(stdout);
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

void keepWindowedGeometry(QWindow& window) {
#if defined(Q_OS_QNX)
  // fullscreen-winmgr resizes a window when it is first posted, which happens
  // after show() returns. Watch for about two seconds and undo it.
  auto* timer = new QTimer(&window);
  auto checks = std::make_shared<int>(0);
  timer->setInterval(50);
  QObject::connect(timer, &QTimer::timeout, &window, [timer, checks, &window]() {
    correctNativeGeometry(window);
    if (++*checks >= 40) timer->deleteLater();
  });
  timer->start();
#else
  (void)window;
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
    // A socket file left by a process that crashed: nobody answered the
    // connect in forwardedToRunningInstance(), so it is stale.
    QLocalServer::removeServer(serverName_);
    if (!server->listen(serverName_)) {
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
    keepWindowedGeometry(window);
  }
  window.raise();
  window.requestActivate();
}

}  // namespace rnqnx::window
