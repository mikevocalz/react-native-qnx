#pragma once

// Platform window behaviour the title bar controls rely on, kept out of the
// Qt Quick chrome so the chrome stays testable without a window manager.
// See docs/adr/0016.
//
// QNX Screen with the stock fullscreen-winmgr:
// - The window manager sets every new window to the display size on its first
//   post and ignores the app's own size and position changes after that.
//   `keepWindowedGeometry` puts a windowed app back to the size Qt asked for.
// - Exactly one window per display is visible: the most recently used one.
//   The launcher reappears only when that window goes away, so minimize
//   destroys the native window and keeps the process, the React tree and its
//   state. `SingleInstance` restores it when the app is launched again.

#include <QObject>
#include <QPointer>
#include <QString>

class QLocalServer;
class QWindow;

namespace rnqnx::window {

// Re-applies the window's Qt geometry to its native window when a window
// manager has resized it behind Qt's back. Checks a few times after the first
// frame, then stops. Does nothing outside QNX or for a full-screen window.
void keepWindowedGeometry(QWindow& window);

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
// native window so the launcher shows. `restoreMinimizedWindow` shows it again.
void minimizeByDestroyingWindow(QWindow& window);
void restoreMinimizedWindow(QWindow& window);

}  // namespace rnqnx::window
