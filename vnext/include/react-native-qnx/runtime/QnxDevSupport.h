#pragma once

#include "react-native-qnx/runtime/QnxDevSupportModules.h"

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <optional>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QQmlEngine;
class QQuickItem;
class QQuickWindow;
class QWebSocket;
QT_END_NAMESPACE

namespace rnqnx::runtime {

/// A JavaScript bundle fetched from Metro, or why it could not be.
struct DevServerBundle {
  bool ok{false};
  QByteArray source;
  /// The URL the bundle came from; it becomes SourceCode.scriptURL, which is
  /// how getDevServer() and asset resolution find Metro.
  QString url;
  QString errorTitle;
  QString errorMessage;
};

/// The development client: loads the bundle from Metro instead of the staged
/// bytecode, implements the native half of Fast Refresh (DevSettings,
/// DevLoadingView, LogBox), keeps Metro's /message connection for `r`/`d`
/// from the terminal, and draws the native loading banner, error screen and
/// developer menu. Only built with RNQNX_ENABLE_DEV_SUPPORT; a release build has
/// none of this code.
///
/// Lifetime: one per window, created before the first React host and kept
/// across reloads. Each reload creates a new QtReactHost; this object only
/// signals and never touches a host itself.
class QnxDevSupport final : public QObject {
  Q_OBJECT

 public:
  struct Options {
    /// http://host:port of the Metro server.
    QUrl server;
    /// Bundle entry without extension, normally "index".
    QString entry{QStringLiteral("index")};
    QString platform{QStringLiteral("qnx")};
    /// App identity, sent as `app=` so Metro logs name the client.
    QString appId;
  };

  /// Parses `--dev-server`. Accepts http(s)://host[:port] with no path, query
  /// or credentials; the port defaults to 8081 like React Native's.
  static std::optional<QUrl> parseServer(const QString& text, QString* error);

  /// `surfaceRoot` is the item the app's React surface mounts into. The
  /// overlays are drawn over exactly that area, so a window title bar stays
  /// usable.
  QnxDevSupport(Options options, QQuickWindow& window, QQmlEngine& engine,
                QQuickItem& surfaceRoot);
  ~QnxDevSupport() override;

  QnxDevSupport(const QnxDevSupport&) = delete;
  QnxDevSupport& operator=(const QnxDevSupport&) = delete;

  QUrl bundleUrl() const;
  /// "index.bundle", what HMRClient.setup() calls the bundle entry.
  QString bundleEntry() const;
  QString host() const;
  int port() const;
  QString scheme() const;
  QString platform() const;
  bool hotLoadingEnabled() const;

  /// Fetches the bundle, showing the loading banner meanwhile. Spins a local
  /// event loop, so the window keeps painting. The first request after Metro
  /// starts can take a minute while it builds the graph.
  DevServerBundle fetchBundle(int timeoutMs = 180000);

  /// The delegate behind DevSettings/DevLoadingView/LogBox. Shared with the
  /// TurboModules of every host; it forwards to this object on the Qt loop.
  std::shared_ptr<DevSupportDelegate> delegate() const;

  /// Parent item for the LogBox surface. Hidden until LogBox.show().
  QQuickItem& logBoxRoot();
  /// Visible while the error screen or the menu is open; touch dispatch must
  /// leave presses inside it to Qt Quick.
  QQuickItem& inputShield();

  void showMessage(const QString& message, const QColor& text, const QColor& background);
  void hideMessage();
  /// The native error screen, for failures before React can show LogBox:
  /// Metro unreachable, a syntax error in the initial bundle, a throw while
  /// the bundle evaluates.
  void showError(const QString& title, const QString& message);
  void hideError();
  void setMenuVisible(bool visible);
  bool menuVisible() const;
  void addMenuItem(const QString& title);
  /// Clears per-runtime state (menu items, banner, error, LogBox) before the
  /// next host starts.
  void resetForReload();
  /// Whether the Metro /message socket is connected.
  bool packagerConnected() const;

 Q_SIGNALS:
  void reloadRequested(const QString& reason);
  void showLogBoxRequested();
  void hideLogBoxRequested();
  void menuItemSelected(const QString& title);
  void hotLoadingToggled(bool enabled);

 public Q_SLOTS:
  void requestReload(const QString& reason);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private Q_SLOTS:
  void onOverlayAction(const QString& action);

 private:
  class Delegate;
  void connectPackager();
  void syncGeometry();
  void setHotLoading(bool enabled);

  Options options_;
  QQuickWindow& window_;
  QQuickItem& surfaceRoot_;
  QQuickItem* overlay_{nullptr};
  QQuickItem* shield_{nullptr};
  QQuickItem* logBoxRoot_{nullptr};
  QNetworkAccessManager* network_{nullptr};
  QWebSocket* packager_{nullptr};
  QTimer packagerRetry_;
  QStringList menuItems_;
  bool hotLoading_{true};
  bool reloadPending_{false};
  bool waitingForServer_{false};
  bool packagerLost_{false};
  std::shared_ptr<Delegate> delegate_;
};

}  // namespace rnqnx::runtime
