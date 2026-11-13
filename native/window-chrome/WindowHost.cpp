#include "WindowHost.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>
#include <QWindow>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#if defined(Q_OS_QNX)
#include <screen/screen.h>
#include <unistd.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif
#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/stat.h>
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

const QStringList& buildVariantSuffixes() {
  static const QStringList suffixes{QStringLiteral("dev"), QStringLiteral("debug"),
                                    QStringLiteral("preview")};
  return suffixes;
}

QString baseAppId(const QString& appId) {
  QString base = appId;
  for (bool stripped = true; stripped;) {
    stripped = false;
    for (const QString& suffix : buildVariantSuffixes()) {
      const QString tail = QLatin1Char('.') + suffix;
      // Keep at least two dotted parts: `org.dev` stays as it is.
      if (base.endsWith(tail) && base.count(QLatin1Char('.')) >= 2) {
        base.chop(tail.size());
        stripped = true;
      }
    }
  }
  return base;
}

QString buildKindName(BuildKind kind) {
  return kind == BuildKind::Dev ? QStringLiteral("dev") : QStringLiteral("release");
}

namespace {

bool processAlive([[maybe_unused]] qint64 pid) {
#if defined(Q_OS_UNIX)
  if (pid <= 0) return false;
  return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#else
  return false;
#endif
}

// SIGTERM only: a stuck app is asked to stop, never killed outright. The
// launcher's own pid and pid 1 are never signalled.
void terminateProcess([[maybe_unused]] qint64 pid) {
#if defined(Q_OS_UNIX)
  if (pid <= 1 || pid == static_cast<qint64>(::getpid())) return;
  ::kill(static_cast<pid_t>(pid), SIGTERM);
#endif
}

QString socketDirectory() {
  QString dir = qEnvironmentVariable("RNQNX_SOCKET_DIR");
#if defined(Q_OS_UNIX)
  // A fixed directory: a session's TMPDIR may differ between an SSH login, the
  // desktop launcher and a daemon, and each would then look in another place.
  if (dir.isEmpty() && QFileInfo(QStringLiteral("/tmp")).isDir()) dir = QStringLiteral("/tmp");
#endif
  if (dir.isEmpty()) dir = QDir::tempPath();
  QDir().mkpath(dir);
  return dir;
}

// A lock on the instance key that any login can take and that disappears with
// its process: fcntl record lock on a world-writable file. Record locks do not
// separate threads of one process, so a process-wide set does.
struct KeyLock {
  static std::mutex& mutex() {
    static std::mutex m;
    return m;
  }
  static std::set<std::string>& held() {
    static std::set<std::string> h;
    return h;
  }
  std::string key;
  int fd{-1};
  ~KeyLock() {
#if defined(Q_OS_UNIX)
    if (fd >= 0) ::close(fd);
#endif
    std::lock_guard<std::mutex> guard(mutex());
    held().erase(key);
  }
};

std::shared_ptr<void> acquireKeyLock([[maybe_unused]] const QString& path, int timeoutMs) {
#if defined(Q_OS_UNIX)
  const std::string key = QFile::encodeName(path).toStdString();
  QElapsedTimer waited;
  waited.start();
  for (;;) {
    {
      std::lock_guard<std::mutex> guard(KeyLock::mutex());
      if (KeyLock::held().count(key) == 0) {
        const int fd = ::open(key.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
        if (fd >= 0) {
          ::fchmod(fd, 0666);
          struct flock lock {};
          lock.l_type = F_WRLCK;
          lock.l_whence = SEEK_SET;
          if (::fcntl(fd, F_SETLK, &lock) == 0) {
            auto result = std::make_shared<KeyLock>();
            result->key = key;
            result->fd = fd;
            KeyLock::held().insert(key);
            return result;
          }
          ::close(fd);
        } else {
          // The lock file cannot be made at all (read-only directory): the
          // socket alone has to arbitrate.
          auto result = std::make_shared<KeyLock>();
          result->key = key;
          KeyLock::held().insert(key);
          return result;
        }
      }
    }
    if (waited.elapsed() >= timeoutMs) return nullptr;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
#else
  return std::make_shared<int>(0);
#endif
}

// "ok pid=<n> kind=<kind>"; an older instance answers a bare "ok".
std::optional<InstanceInfo> parseReply(const QByteArray& line) {
  const QList<QByteArray> parts = line.trimmed().split(' ');
  if (parts.isEmpty() || parts.first() != QByteArrayLiteral("ok")) return std::nullopt;
  InstanceInfo info;
  for (const QByteArray& part : parts) {
    if (part.startsWith("pid=")) info.pid = part.mid(4).toLongLong();
    if (part.startsWith("kind=")) info.kind = QString::fromLatin1(part.mid(5));
  }
  return info;
}

}  // namespace

SingleInstance::SingleInstance(const QString& appId, BuildKind kind, QObject* parent)
    : QObject(parent),
      baseId_(baseAppId(appId)),
      kind_(kind),
      serverName_(socketNameFor(appId)),
      alive_(processAlive),
      terminate_(terminateProcess) {}

SingleInstance::~SingleInstance() {
  if (server_) server_->close();
}

QString SingleInstance::socketNameFor(const QString& appId) {
  return socketDirectory() + QStringLiteral("/rnqnx-%1").arg(baseAppId(appId));
}

QStringList SingleInstance::legacySocketNamesFor(const QString& appId) {
  const QString base = baseAppId(appId);
  const QString user = currentUser();
  QStringList names;
  for (const QString& dir : {socketDirectory(), QDir::tempPath()}) {
    for (const QString& id : {base, base + QStringLiteral(".dev")}) {
      const QString path = QStringLiteral("%1/rnqnx-%2-%3").arg(dir, id, user);
      if (!names.contains(path)) names.append(path);
    }
  }
  return names;
}

// The primary path; the per-user fallback a launch uses when a stale socket of
// another login cannot be removed; then older builds' names.
QStringList SingleInstance::candidateSockets() const {
  QStringList names{serverName_, serverName_ + QLatin1Char('.') + currentUser()};
  names.removeDuplicates();
  names += legacySocketNamesFor(baseId_);
  return names;
}

void SingleInstance::setProcessProbe(std::function<bool(qint64)> alive,
                                     std::function<void(qint64)> terminate) {
  alive_ = std::move(alive);
  terminate_ = std::move(terminate);
}

// Sends one command. nullopt: nothing listens there. A connected server that
// does not answer (an older build and an unknown command) gives pid 0.
std::optional<InstanceInfo> SingleInstance::talk(const QString& socketName, const char* command) {
  QLocalSocket socket;
  socket.connectToServer(socketName);
  if (!socket.waitForConnected(500)) return std::nullopt;
  socket.write(command);
  socket.write("\n");
  if (!socket.waitForBytesWritten(500)) return std::nullopt;
  // Wait for the answer so the request is not lost when this process exits
  // straight away.
  if (!socket.canReadLine()) socket.waitForReadyRead(1000);
  InstanceInfo info;
  if (socket.canReadLine()) {
    if (const auto parsed = parseReply(socket.readLine())) info = *parsed;
  }
  socket.disconnectFromServer();
  return info;
}

bool SingleInstance::forwardedToRunningInstance() {
  const QStringList names = candidateSockets();
  for (const QString& name : names)
    if (talk(name, "restore")) return true;
  return false;
}

std::optional<InstanceInfo> SingleInstance::runningInstanceInfo() {
  const QStringList names = candidateSockets();
  for (const QString& name : names)
    if (const auto info = talk(name, "info")) return info;
  return std::nullopt;
}

bool SingleInstance::waitForExit(qint64 pid, int timeoutMs) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (alive_(pid)) {
    if (elapsed.elapsed() >= timeoutMs) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return true;
}

ReplaceResult SingleInstance::replaceRunningInstance(int quitTimeoutMs, int termTimeoutMs) {
  const QStringList names = candidateSockets();
  bool anyone = false;
  for (const QString& name : names) {
    const auto info = talk(name, "quit");
    if (!info) continue;
    anyone = true;
    if (info->pid <= 0) {
      // An older build: it takes `restore` only, so there is no pid to wait for.
      std::fprintf(stderr,
                   "RNQNX_SINGLE_INSTANCE_WARNING %s does not report a pid; stop it with the "
                   "app's launcher script or by pid\n",
                   name.toUtf8().constData());
      return ReplaceResult::Failed;
    }
    std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE replacing pid %lld (%s)\n",
                 static_cast<long long>(info->pid), info->kind.toUtf8().constData());
    if (waitForExit(info->pid, quitTimeoutMs)) continue;
    std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE_WARNING pid %lld ignored quit; sending SIGTERM\n",
                 static_cast<long long>(info->pid));
    // The pid could have been reused if the old process exited meanwhile: only
    // signal it while the same instance still answers with that pid.
    const auto again = talk(name, "info");
    if (!again || again->pid != info->pid) continue;
    terminate_(info->pid);
    if (!waitForExit(info->pid, termTimeoutMs)) {
      std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE_WARNING pid %lld is still running\n",
                   static_cast<long long>(info->pid));
      return ReplaceResult::Failed;
    }
  }
  return anyone ? ReplaceResult::Replaced : ReplaceResult::NoneRunning;
}

bool SingleInstance::listen() {
  // Qt 6 lets a second listen() take over the name of a live server, which
  // would leave the first instance unreachable and allow two windows. Look
  // for a live owner on every candidate path and leave it alone.
  const auto liveOwner = [this]() {
    for (const QString& name : candidateSockets()) {
      QLocalSocket probe;
      probe.connectToServer(name);
      if (probe.waitForConnected(200)) {
        probe.abort();
        std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE_WARNING %s is owned by a running instance\n",
                     name.toUtf8().constData());
        return true;
      }
    }
    return false;
  };
  if (liveOwner()) return false;
  // Probe, remove and listen are one step: another launch starting at the same
  // moment (any login) waits here, then finds this socket live. The lock goes
  // with its process, so it is never stale.
  auto lock = acquireKeyLock(serverName_ + QStringLiteral(".lock"), 1500);
  if (!lock) {
    std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE_WARNING %s is being started by another launch\n",
                 serverName_.toUtf8().constData());
    return false;
  }
  if (liveOwner()) return false;
  // Nobody answers: a socket file that is still there is stale (a crashed
  // process) and goes. If another login's stale file cannot be removed, this
  // user's own fallback path takes its place (candidateSockets() covers it).
  QString path = serverName_;
  QLocalServer::removeServer(path);
  auto* server = new QLocalServer(this);
  server->setSocketOptions(QLocalServer::WorldAccessOption);
  bool listening = server->listen(path);
  if (!listening) {
    path = serverName_ + QLatin1Char('.') + currentUser();
    QLocalServer::removeServer(path);
    listening = server->listen(path);
  }
  if (!listening) {
    std::fprintf(stderr, "RNQNX_SINGLE_INSTANCE_WARNING cannot listen on %s: %s\n",
                 serverName_.toUtf8().constData(), server->errorString().toUtf8().constData());
    delete server;
    return false;
  }
  lock_ = std::move(lock);
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
        const QByteArray command = socket->readLine().trimmed();
        const bool known = command == "restore" || command == "info" || command == "quit";
        socket->write(known ? QByteArrayLiteral("ok pid=") +
                                  QByteArray::number(static_cast<qlonglong>(QCoreApplication::applicationPid())) +
                                  QByteArrayLiteral(" kind=") + buildKindName(kind_).toLatin1() +
                                  QByteArrayLiteral("\n")
                            : QByteArrayLiteral("error unknown command\n"));
        socket->flush();
        if (command == "restore") Q_EMIT restoreRequested();
        if (command == "quit") Q_EMIT quitRequested();
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
