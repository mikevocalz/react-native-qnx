#pragma once

// Owns the client-drawn title bar and the item the Fabric root mounts into.
// Lives on the Qt GUI thread with the window and never calls into JavaScript.
// QtReactHost currently runs JavaScript on that same thread, so a blocked JS
// call also blocks these controls until the host gets its own JS thread.
// See ADR-0016.

#include "WindowChromeModel.h"

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>

#include <QStringList>
#include <QUrl>

#include <memory>

class QQmlEngine;
class QQuickItem;
#include <QQuickWindow>

namespace rnqnx::window {

class WindowChrome final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString controlsLayout READ controlsLayout CONSTANT)
  // The set of controls is fixed for the window's life; only their geometry
  // changes. Keeping the model constant keeps the Repeater's delegates (and
  // keyboard focus) across resizes.
  Q_PROPERTY(QVariantList controls READ controls CONSTANT)
  Q_PROPERTY(QVariantMap controlGeometry READ controlGeometry NOTIFY layoutChanged)
  Q_PROPERTY(QRectF titleRect READ titleRect NOTIFY layoutChanged)
  Q_PROPERTY(bool titleCentered READ titleCentered CONSTANT)
  Q_PROPERTY(bool barVisible READ barVisible CONSTANT)
  Q_PROPERTY(qreal barHeight READ barHeight CONSTANT)
  Q_PROPERTY(qreal windowWidth READ windowWidth NOTIFY layoutChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(bool active READ active NOTIFY stateChanged)
  Q_PROPERTY(bool maximized READ maximized NOTIFY stateChanged)
  Q_PROPERTY(bool systemMoveAvailable READ systemMoveAvailable CONSTANT)
  // Glyphs stay visible so controls are not told apart by colour alone. Only
  // cocoa, where hover exists and is the platform convention, reveals them on
  // hover or focus.
  Q_PROPERTY(bool alwaysShowGlyphs READ alwaysShowGlyphs CONSTANT)
  // App icon drawn beside the title; empty URL draws none.
  Q_PROPERTY(QUrl iconSource READ iconSource NOTIFY iconChanged)
  // A double click on the bar toggles maximize only when that control is live.
  Q_PROPERTY(bool maximizeAvailable READ maximizeAvailable CONSTANT)

 public:
  // Creates the chrome item and the content root inside `window`. Throws
  // std::runtime_error when the QML fails to load.
  WindowChrome(QQuickWindow& window, QQmlEngine& engine, WindowChromeConfig config,
               WindowCapabilities capabilities);
  ~WindowChrome() override;

  WindowChrome(const WindowChrome&) = delete;
  WindowChrome& operator=(const WindowChrome&) = delete;

  // The Fabric root's parent. Starts below the bar in the `default` style and
  // tracks the window size.
  QQuickItem& contentRoot() const;
  QQuickItem* chromeItem() const;
  const ChromeLayout& layout() const { return layout_; }
  const WindowCapabilities& capabilities() const { return capabilities_; }

  QString controlsLayout() const { return nameOf(config_.controls); }
  QVariantList controls() const { return controls_; }
  QVariantMap controlGeometry() const;
  QRectF titleRect() const { return layout_.titleRect; }
  bool titleCentered() const { return layout_.titleAlignment == Qt::AlignHCenter; }
  bool barVisible() const { return layout_.barVisible; }
  qreal barHeight() const { return layout_.barHeight; }
  qreal windowWidth() const;
  QString title() const;
  bool active() const;
  bool maximized() const;
  bool systemMoveAvailable() const { return capabilities_.systemMove; }
  bool alwaysShowGlyphs() const { return capabilities_.platform != QLatin1String("cocoa"); }
  QUrl iconSource() const { return iconSource_; }
  bool maximizeAvailable() const;
  void setIconSource(const QUrl& source);

  // One line per hidden or disabled control, for logs and device receipts.
  QStringList describeHiddenControls() const;

  Q_INVOKABLE void trigger(const QString& action);
  Q_INVOKABLE void beginSystemMove();

 Q_SIGNALS:
  void layoutChanged();
  void titleChanged();
  void stateChanged();
  // Emitted before the action runs. Tests and receipts log it.
  void actionTriggered(const QString& action);
  // A disabled control was activated; nothing ran.
  void actionUnavailable(const QString& action, const QString& reason);
  void iconChanged();

 private:
  void relayout();

  QPointer<QQuickWindow> window_;
  WindowChromeConfig config_;
  WindowCapabilities capabilities_;
  ChromeLayout layout_;
  QVariantList controls_;
  QUrl iconSource_;
  std::unique_ptr<QQuickItem> content_;
  std::unique_ptr<QQuickItem> chrome_;
};

}  // namespace rnqnx::window
