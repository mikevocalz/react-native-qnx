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

class QLocalServer;
class QWindow;

namespace rnqnx::window {

// True when the window is maximized or full screen, or covers its screen's
// available area because a window manager sized it so.
bool windowExpanded(const QWindow& window);

// One running copy per app id and user. The first process listens on a local
// socket; a second launch hands its request to the first and exits.
class SingleInstance final : public QObject {
  Q_OBJECT

 public:
  explicit SingleInstance(const QString& appId, QObject* parent = nullptr);
  ~SingleInstance() override;

  // True when another process with the same app id is running and took the
  // restore request. The caller should then exit without creating a window.
  bool forwardedToRunningInstance();
  // Starts listening. Returns false (and logs) when the socket cannot be made;
  // the app then runs without the relaunch-to-restore path.
  bool listen();
  QString serverName() const { return serverName_; }

 Q_SIGNALS:
  // A later launch of the same app asked this one to show its window.
  void restoreRequested();

 private:
  void acceptConnections();

  QString serverName_;
  QPointer<QLocalServer> server_;
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
