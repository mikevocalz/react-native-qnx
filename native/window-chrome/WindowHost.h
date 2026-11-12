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

// Minimize the way fullscreen-winmgr understands it: hide and destroy the
// native window so the launcher shows.
void minimizeByDestroyingWindow(QWindow& window);
// Shows a minimized window again. A window that still exists but sits behind
// another one (the launcher, or an app picked with Alt+Tab) is recreated,
// because only a new window becomes the manager's front window.
void restoreMinimizedWindow(QWindow& window);

}  // namespace rnqnx::window
