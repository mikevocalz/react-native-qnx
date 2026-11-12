#include "WindowChrome.h"

#include "WindowChromeQml.generated.h"
#include "WindowHost.h"

#include <QCursor>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace rnqnx::window {

WindowChrome::WindowChrome(QQuickWindow& window, QQmlEngine& engine, WindowChromeConfig config,
                           WindowCapabilities capabilities)
    : window_(&window), config_(std::move(config)), capabilities_(std::move(capabilities)) {
  contained_ = capabilities_.containedWindow && config_.initialSize.has_value() &&
               config_.titleBarStyle != TitleBarStyle::None;
  movesWindow_ = !contained_ && capabilities_.clientMoveResize &&
                 config_.titleBarStyle != TitleBarStyle::None;
  frame_ = QRectF(0, 0, window.width(), window.height());
  layout_ = layoutChrome(config_, capabilities_, frame_.width());
  for (const auto& slot : layout_.controls) {
    controls_.append(QVariantMap{{QStringLiteral("action"), nameOf(slot.action)},
                                 {QStringLiteral("name"), slot.accessibleName},
                                 {QStringLiteral("enabled"), slot.enabled}});
  }

  content_ = std::make_unique<QQuickItem>();
  content_->setObjectName(QStringLiteral("rnqnxContentRoot"));
  content_->setParentItem(window.contentItem());
  content_->setClip(true);

  QQmlComponent component(&engine);
  component.setData(QByteArray(kWindowChromeQml),
                    QUrl(QStringLiteral("qrc:/rnqnx/WindowChrome.qml")));
  QObject* created = component.createWithInitialProperties(
      {{QStringLiteral("chrome"), QVariant::fromValue(static_cast<QObject*>(this))}});
  auto* item = qobject_cast<QQuickItem*>(created);
  if (item == nullptr) {
    delete created;
    throw std::runtime_error(("Window chrome QML failed to load: " + component.errorString())
                                 .toStdString());
  }
  chrome_.reset(item);
  chrome_->setParentItem(window.contentItem());

  connect(&window, &QWindow::widthChanged, this, &WindowChrome::relayout);
  connect(&window, &QWindow::heightChanged, this, &WindowChrome::relayout);
  connect(&window, &QWindow::windowTitleChanged, this, &WindowChrome::titleChanged);
  connect(&window, &QWindow::activeChanged, this, &WindowChrome::stateChanged);
  connect(&window, &QWindow::windowStateChanged, this, &WindowChrome::stateChanged);
  // A window manager can resize the window to the whole screen without a
  // state change; the expand control follows the size, not the last press.
  connect(&window, &QWindow::widthChanged, this, &WindowChrome::stateChanged);
  connect(&window, &QWindow::heightChanged, this, &WindowChrome::stateChanged);
  if (contained_) {
    // The display around the frame. Dark like the QNX launcher.
    window.setColor(QColor(QStringLiteral("#0b1016")));
  }
  if (contained_ || movesWindow_) window.installEventFilter(this);
  relayout();
}

WindowChrome::~WindowChrome() {
  // Items go before the window's content item; Fabric unmounts children of
  // content_ before this object is destroyed (it outlives the host in main).
  chrome_.reset();
  content_.reset();
}

QQuickItem& WindowChrome::contentRoot() const { return *content_; }

QQuickItem* WindowChrome::chromeItem() const { return chrome_.get(); }

QVariantMap WindowChrome::controlGeometry() const {
  QVariantMap result;
  for (const auto& slot : layout_.controls) {
    result.insert(nameOf(slot.action), QVariantMap{{QStringLiteral("x"), slot.hitRect.x()},
                                                   {QStringLiteral("y"), slot.hitRect.y()},
                                                   {QStringLiteral("width"), slot.hitRect.width()},
                                                   {QStringLiteral("height"), slot.hitRect.height()}});
  }
  return result;
}

qreal WindowChrome::windowWidth() const { return frame_.width(); }

QString WindowChrome::title() const { return window_ ? window_->title() : QString(); }

bool WindowChrome::active() const { return window_ && window_->isActive(); }

bool WindowChrome::maximized() const {
  if (contained_) return frameExpanded_;
  return window_ && windowExpanded(*window_);
}

QStringList WindowChrome::describeHiddenControls() const {
  QStringList lines;
  for (const auto& hidden : layout_.hidden) {
    lines.append(QStringLiteral("RNQNX_WINDOW_CHROME platform=%1 hidden=%2 reason=%3")
                     .arg(capabilities_.platform, nameOf(hidden.action), hidden.reason));
  }
  for (const auto& disabled : layout_.disabled) {
    lines.append(QStringLiteral("RNQNX_WINDOW_CHROME platform=%1 disabled=%2 reason=%3")
                     .arg(capabilities_.platform, nameOf(disabled.action), disabled.reason));
  }
  return lines;
}

void WindowChrome::trigger(const QString& action) {
  if (!window_) return;
  // Only controls that are laid out can act; a stale QML call for a hidden
  // control does nothing.
  const ControlSlot* found = nullptr;
  for (const auto& slot : layout_.controls)
    if (nameOf(slot.action) == action) found = &slot;
  if (found == nullptr) return;
  if (!found->enabled) {
    Q_EMIT actionUnavailable(action, found->reason);
    return;
  }
  Q_EMIT actionTriggered(action);
  if (action == QLatin1String("close")) {
    window_->close();
  } else if (action == QLatin1String("minimize")) {
    if (capabilities_.clientMoveResize) {
      // rnqnx-winmgr hides the window and lists it in its dock.
      sendWindowManagerRequest(*window_, "minimize");
    } else if (capabilities_.platform == QLatin1String("qnx")) {
      // QQnxWindow::minimize() does nothing; see WindowHost.h.
      minimizeByDestroyingWindow(*window_);
    } else {
      window_->showMinimized();
    }
  } else if (action == QLatin1String("maximize")) {
    if (contained_) {
      frameExpanded_ = !frameExpanded_;
      relayout();
      Q_EMIT stateChanged();
    } else if (maximized()) {
      window_->showNormal();
    } else {
      window_->showMaximized();
    }
  }
}

bool WindowChrome::maximizeAvailable() const {
  for (const auto& slot : layout_.controls)
    if (slot.action == WindowAction::Maximize && slot.enabled) return true;
  return false;
}

void WindowChrome::setIconSource(const QUrl& source) {
  if (source == iconSource_) return;
  iconSource_ = source;
  Q_EMIT iconChanged();
}

void WindowChrome::beginSystemMove() {
  if (window_ && capabilities_.systemMove) window_->startSystemMove();
}

void WindowChrome::relayout() {
  if (!window_) return;
  const QRectF whole(0, 0, window_->width(), window_->height());
  if (!contained_ || frameExpanded_) {
    frame_ = whole;
  } else {
    if (normalFrame_.isEmpty()) {
      const QSize size = clampWindowSize(*config_.initialSize, whole.size().toSize());
      normalFrame_ = QRectF(QPointF((whole.width() - size.width()) / 2,
                                    (whole.height() - size.height()) / 2),
                            QSizeF(size));
    }
    // The window can be smaller than the frame before the window manager has
    // sized it to the display; never draw outside it.
    frame_ = normalFrame_.intersected(whole);
    if (frame_.isEmpty()) frame_ = whole;
  }
  layout_ = layoutChrome(config_, capabilities_, frame_.width());
  content_->setPosition(QPointF(frame_.x(), frame_.y() + layout_.contentInset));
  content_->setSize(QSizeF(frame_.width(), std::max<qreal>(0, frame_.height() - layout_.contentInset)));
  Q_EMIT layoutChanged();
}

void WindowChrome::setResizeHint(Qt::Edges edges, QPointF pointer) {
  QString hint;
  Qt::CursorShape shape = Qt::ArrowCursor;
  const bool horizontal = edges & (Qt::LeftEdge | Qt::RightEdge);
  const bool vertical = edges & (Qt::TopEdge | Qt::BottomEdge);
  if (horizontal && vertical) {
    const bool mainDiagonal = (edges & Qt::LeftEdge) ? bool(edges & Qt::TopEdge) : bool(edges & Qt::BottomEdge);
    hint = mainDiagonal ? QStringLiteral("diagonal") : QStringLiteral("antidiagonal");
    shape = mainDiagonal ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
  } else if (horizontal) {
    hint = QStringLiteral("horizontal");
    shape = Qt::SizeHorCursor;
  } else if (vertical) {
    hint = QStringLiteral("vertical");
    shape = Qt::SizeVerCursor;
  }
  if (hint != resizeHint_ && window_) {
    // Where the QPA has resize cursors (not QNX Screen) this is all it takes.
    if (hint.isEmpty())
      window_->unsetCursor();
    else
      window_->setCursor(QCursor(shape));
  }
  if (hint == resizeHint_ && (hint.isEmpty() || pointer == pointer_)) return;
  resizeHint_ = hint;
  pointer_ = pointer;
  Q_EMIT resizeHintChanged();
}

bool WindowChrome::eventFilter(QObject* watched, QEvent* event) {
  if (watched == window_ && (contained_ || movesWindow_)) {
    switch (event->type()) {
      case QEvent::MouseButtonPress:
      case QEvent::MouseMove:
      case QEvent::MouseButtonRelease:
        return handleFramePointer(event);
      default:
        break;
    }
  }
  return QObject::eventFilter(watched, event);
}

// Returns true when the event moved or resized the frame and must not reach
// the items under the pointer.
bool WindowChrome::handleFramePointer(QEvent* event) {
  auto* mouse = static_cast<QMouseEvent*>(event);
  const QPointF point = mouse->position();
  const QPointF local = point - frame_.topLeft();
  std::vector<QRectF> controls;
  for (const auto& slot : layout_.controls) controls.push_back(slot.hitRect);

  // A contained frame moves inside the window; otherwise the native window
  // moves on its screen.
  const bool expanded = contained_ ? frameExpanded_ : maximized();
  if (event->type() == QEvent::MouseButtonPress) {
    if (mouse->button() != Qt::LeftButton || expanded) return false;
    const Qt::Edges edges = resizeEdgesAt(local, frame_.size(), controls);
    dragStart_ = mouse->globalPosition();
    dragStartFrame_ = contained_ ? frame_.toRect() : window_->geometry();
    if (edges) {
      drag_ = Drag::Resize;
      dragEdges_ = edges;
      return true;
    }
    bool onControl = false;
    for (const QRectF& rect : controls) onControl = onControl || rect.contains(local);
    if (layout_.barHeight > 0 && local.y() >= 0 && local.y() < layout_.barHeight && local.x() >= 0 &&
        local.x() < frame_.width() && !onControl) {
      drag_ = Drag::PendingMove;  // Becomes a move once the pointer travels.
    }
    // Outside the frame there is nothing to press.
    return !frame_.contains(point);
  }

  if (event->type() == QEvent::MouseMove) {
    const QPoint delta = (mouse->globalPosition() - dragStart_).toPoint();
    QRect screen(0, 0, window_->width(), window_->height());
    if (!contained_) screen = window_->screen() ? window_->screen()->availableGeometry() : QRect();
    if (drag_ == Drag::PendingMove && delta.manhattanLength() >= 4) drag_ = Drag::Move;
    if (drag_ == Drag::Move || drag_ == Drag::Resize) {
      const QRect geometry = drag_ == Drag::Move
          ? movedGeometry(dragStartFrame_, delta, screen)
          : resizedGeometry(dragStartFrame_, dragEdges_, delta,
                            QSize(kMinWindowWidth, kMinWindowHeight), screen);
      if (contained_) {
        normalFrame_ = QRectF(geometry);
        relayout();
        if (drag_ == Drag::Resize) setResizeHint(dragEdges_, point);
      } else {
        if (drag_ == Drag::Move)
          window_->setPosition(geometry.topLeft());
        else
          window_->setGeometry(geometry);
        // The event's window coordinates predate the move; the pointer's
        // place in the moved window comes from its global position.
        if (drag_ == Drag::Resize)
          setResizeHint(dragEdges_, mouse->globalPosition() - QPointF(geometry.topLeft()));
      }
      return true;
    }
    if (drag_ == Drag::None && !(mouse->buttons() & Qt::LeftButton))
      setResizeHint(expanded ? Qt::Edges() : resizeEdgesAt(local, frame_.size(), controls), point);
    return false;
  }

  // Release.
  const bool consumed = drag_ == Drag::Move || drag_ == Drag::Resize;
  drag_ = Drag::None;
  if (consumed) {
    setResizeHint(resizeEdgesAt(local, frame_.size(), controls), point);
    const QRect placed = contained_ ? frame_.toRect() : window_->geometry();
    std::printf("RNQNX_WINDOW_FRAME %dx%d+%d+%d\n", placed.width(), placed.height(), placed.x(),
                placed.y());
    std::fflush(stdout);
  }
  return consumed;
}

}  // namespace rnqnx::window
