#pragma once

// Platform window behaviour the title bar controls rely on, kept out of the
// Qt Quick chrome so the chrome stays testable without a window manager.
// See docs/adr/0016.
//
// QNX Screen with the stock fullscreen-winmgr (QNX 8.0 screen demos
// 5.0.0.00343; disassembly notes in docs/getting-started/pi400-desktop-app.md):
// - Every time a window posts a frame, the manager sets its size to the
//   display size. A windowed app therefore draws its window frame inside a
//   display-sized native window (WindowChrome's contained window).
// - Exactly one window per display is visible: the most recently created or
//   switched-to one. The manager has no message that brings the launcher
//   forward (switching needs a tag, and the launcher's window has none), so
//   minimize destroys the native window and keeps the process, the React tree
//   and its state. `SingleInstance` restores it when the app is launched
//   again, from its launcher tile or its run script.
//
// QNX Screen with rnqnx-winmgr (tools/qnx-winmgr, a drop-in replacement that
// keeps the launcher on screen as the desktop):
// - App windows keep the size and position the app sets, so the app uses a
//   real native window of its own size and moves and resizes it itself.
// - The manager sets each app window's SCREEN_PROPERTY_MANAGER_STRING to
//   "rnqnx-winmgr windowed" and takes "rnqnx=minimize" and "rnqnx=restore"
//   as SCREEN_EVENT_MANAGER messages, the way demolauncher talks to it.

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QLocalServer;
class QWindow;

namespace rnqnx::window {

// True when the window is maximized or full screen, or covers its screen's
// available area because a window manager sized it so.
bool windowExpanded(const QWindow& window);

// Build-variant suffixes an app id can carry (`org.example.app.dev`). The
// instance key is the id without them, so a development client and the
// release build of one app are the same app. This is the one list.
const QStringList& buildVariantSuffixes();

// The id with every trailing build-variant suffix removed.
QString baseAppId(const QString& appId);

// Release or development build, reported over the socket.
enum class BuildKind { Release, Dev };
QString buildKindName(BuildKind kind);

// What a running instance says about itself.
struct InstanceInfo {
  qint64 pid{0};
  QString kind;  // "release" or "dev"
};

// How a launch treats an instance that is already running.
enum class ReplaceResult {
  NoneRunning,  // nothing answered; start normally
  Replaced,     // the old instance is gone; start normally
  Failed,       // it would not go (quit and SIGTERM both failed); do not start
};

// One running copy per app, whatever its build kind. The key is the base app
// id only: a single-user device, so no uid in the name, and a dev client and
// the release build share it. The first process listens on a local socket
// `rnqnx-<base id>`. A later launch either hands it a restore request and
// exits, or (replace) asks it to quit, waits, escalates to SIGTERM, then
// takes over. Socket commands, one per line:
//   restore  show the window                -> "ok pid=<n> kind=<kind>"
//   info     report only                    -> "ok pid=<n> kind=<kind>"
//   quit     close the app gracefully       -> "ok pid=<n> kind=<kind>"
class SingleInstance final : public QObject {
  Q_OBJECT

 public:
  explicit SingleInstance(const QString& appId, BuildKind kind = BuildKind::Release,
                          QObject* parent = nullptr);
  ~SingleInstance() override;

  // The socket name for an app id (variant suffixes are ignored).
  static QString socketNameFor(const QString& appId);
  // Names an older build may be listening on (`rnqnx-<id>-<uid>`, with and
  // without the `.dev` suffix). They understand `restore` only.
  static QStringList legacySocketNamesFor(const QString& appId);

  // True when another process with the same app took the restore request.
  // The caller should then exit without creating a window.
  bool forwardedToRunningInstance();
  // What a running instance reports, if one answers.
  std::optional<InstanceInfo> runningInstanceInfo();
  // Asks the running instance to quit and waits up to `quitTimeoutMs` for its
  // process to exit; then sends SIGTERM to the pid it reported and waits up
  // to `termTimeoutMs`. Never sends SIGKILL.
  ReplaceResult replaceRunningInstance(int quitTimeoutMs = 5000, int termTimeoutMs = 3000);
  // Starts listening. Returns false (and logs) when the socket cannot be made;
  // the app then runs without the relaunch-to-restore path.
  bool listen();
  QString serverName() const { return serverName_; }

  // Tests replace how a pid is checked and signalled.
  void setProcessProbe(std::function<bool(qint64)> alive,
                       std::function<void(qint64)> terminate);

 Q_SIGNALS:
  // A later launch of the same app asked this one to show its window.
  void restoreRequested();
  // A later launch with --replace asked this one to quit.
  void quitRequested();

 private:
  void acceptConnections();
  std::optional<InstanceInfo> talk(const QString& socketName, const char* command);
  bool waitForExit(qint64 pid, int timeoutMs);

  QString baseId_;
  BuildKind kind_;
  QString serverName_;
  QPointer<QLocalServer> server_;
  std::function<bool(qint64)> alive_;
  std::function<void(qint64)> terminate_;
};

// Asks the Screen window manager whether it keeps app windows at their own
// size (rnqnx-winmgr). Creates the native window when it does not exist yet
// and waits up to `timeoutMs` for the manager's answer. RNQNX_WINDOW_MANAGER
// set to `windowed` or `fullscreen` skips the question. Always false off QNX.
bool windowManagerKeepsWindowSize(QWindow& window, int timeoutMs = 500);

// Writes the window's position to its Screen window. Under a window manager
// Screen drops an app's own position until the manager grants self layout,
// which rnqnx-winmgr does when the window is created; Qt does not send the
// position again unless the geometry changes. Does nothing off QNX.
void applyNativePosition(QWindow& window);

// Sends `request` ("minimize", "restore") to rnqnx-winmgr as the manager
// message "rnqnx=<request>". Does nothing off QNX or before the native window
// exists.
void sendWindowManagerRequest(QWindow& window, const char* request);

// Minimize the way fullscreen-winmgr understands it: hide and destroy the
// native window so the launcher shows.
void minimizeByDestroyingWindow(QWindow& window);
// Shows a minimized window again. Under rnqnx-winmgr
// (`sizeKeepingManager`) the manager shows and raises it. Under
// fullscreen-winmgr a window that still exists but sits behind another one
// (the launcher, or an app picked with Alt+Tab) is recreated, because only a
// new window becomes that manager's front window.
void restoreMinimizedWindow(QWindow& window, bool sizeKeepingManager = false);

}  // namespace rnqnx::window
