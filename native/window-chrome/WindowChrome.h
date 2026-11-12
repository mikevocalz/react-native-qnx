#pragma once

// Owns the client-drawn title bar and the item the Fabric root mounts into.
// Lives on the Qt GUI thread with the window; it never calls into JavaScript,
// so the controls keep working while the JS thread is busy. See ADR-0016.

#include "WindowChromeModel.h"

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QVariantList>

#include <QStringList>

#include <memory>

class QQmlEngine;
class QQuickItem;
#include <QQuickWindow>

namespace rnqnx::window {

class WindowChrome final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString controlsLayout READ controlsLayout CONSTANT)
  Q_PROPERTY(QVariantList controls READ controls NOTIFY layoutChanged)
  Q_PROPERTY(QRectF titleRect READ titleRect NOTIFY layoutChanged)
  Q_PROPERTY(bool titleCentered READ titleCentered CONSTANT)
  Q_PROPERTY(bool barVisible READ barVisible CONSTANT)
  Q_PROPERTY(qreal barHeight READ barHeight CONSTANT)
  Q_PROPERTY(qreal windowWidth READ windowWidth NOTIFY layoutChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(bool active READ active NOTIFY stateChanged)
  Q_PROPERTY(bool maximized READ maximized NOTIFY stateChanged)
  Q_PROPERTY(bool systemMoveAvailable READ systemMoveAvailable CONSTANT)

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
  QVariantList controls() const;
  QRectF titleRect() const { return layout_.titleRect; }
  bool titleCentered() const { return layout_.titleAlignment == Qt::AlignHCenter; }
  bool barVisible() const { return layout_.barVisible; }
  qreal barHeight() const { return layout_.barHeight; }
  qreal windowWidth() const;
  QString title() const;
  bool active() const;
  bool maximized() const;
  bool systemMoveAvailable() const { return capabilities_.systemMove; }

  // One line per hidden control, for logs and device receipts.
  QStringList describeHiddenControls() const;

  Q_INVOKABLE void trigger(const QString& action);
  Q_INVOKABLE void beginSystemMove();

 signals:
  void layoutChanged();
  void titleChanged();
  void stateChanged();
  // Emitted before the action runs. Tests and receipts log it.
  void actionTriggered(const QString& action);

 private:
  void relayout();

  QPointer<QQuickWindow> window_;
  WindowChromeConfig config_;
  WindowCapabilities capabilities_;
  ChromeLayout layout_;
  std::unique_ptr<QQuickItem> content_;
  std::unique_ptr<QQuickItem> chrome_;
};

}  // namespace rnqnx::window
