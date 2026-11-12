#pragma once

// Owns the client-drawn title bar and the item the Fabric root mounts into.
//
// Contained window (WindowCapabilities::containedWindow, QNX): the native
// window stays at the display size, because fullscreen-winmgr sets it there
// on every post, and the chrome draws the app's window as a frame inside it.
// The frame opens at initialSize, centred; dragging the bar moves it, dragging
// an edge or corner resizes it, and the maximize control toggles it between
// that size and the whole display. The area around the frame is the desktop
// colour; the window manager hides every other window anyway.
// Lives on the Qt GUI thread with the window and never calls into JavaScript.
// QtReactHost currently runs JavaScript on that same thread, so a blocked JS
// call also blocks these controls until the host gets its own JS thread.
// See ADR-0016.

#include "WindowChromeModel.h"

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRect>
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
  // True while the window fills its screen: maximized or full screen, or sized
  // to the screen by a window manager. Drives the expand/restore glyph.
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
  // Where the window frame is drawn, in window coordinates: the whole window
  // except for a contained window.
  Q_PROPERTY(QRectF frameRect READ frameRect NOTIFY layoutChanged)
  Q_PROPERTY(bool contained READ contained CONSTANT)
  // Edge or corner under the pointer of a contained window, for the resize
  // hint drawn where the platform has no resize cursors: "", "horizontal",
  // "vertical", "diagonal" (top-left/bottom-right) or "antidiagonal".
  Q_PROPERTY(QString resizeHint READ resizeHint NOTIFY resizeHintChanged)
  Q_PROPERTY(QPointF pointer READ pointer NOTIFY resizeHintChanged)

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
  QRectF frameRect() const { return frame_; }
  bool contained() const { return contained_; }
  QString resizeHint() const { return resizeHint_; }
  QPointF pointer() const { return pointer_; }
  // The contained frame when not maximized: initialSize centred until the
  // user moves or resizes it.
  QRectF normalFrame() const { return normalFrame_; }

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
  void resizeHintChanged();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void relayout();
  bool handleFramePointer(QEvent* event);
  void setResizeHint(Qt::Edges edges, QPointF pointer);

  QPointer<QQuickWindow> window_;
  WindowChromeConfig config_;
  WindowCapabilities capabilities_;
  ChromeLayout layout_;
  QVariantList controls_;
  QUrl iconSource_;
  bool contained_{false};
  bool frameExpanded_{false};
  QRectF frame_;
  QRectF normalFrame_;
  QString resizeHint_;
  QPointF pointer_;
  // Pointer drag on a contained frame: Move on the bar, Resize on an edge.
  enum class Drag { None, PendingMove, Move, Resize };
  Drag drag_{Drag::None};
  Qt::Edges dragEdges_;
  QPointF dragStart_;
  QRect dragStartFrame_;
  std::unique_ptr<QQuickItem> content_;
  std::unique_ptr<QQuickItem> chrome_;
};

}  // namespace rnqnx::window
