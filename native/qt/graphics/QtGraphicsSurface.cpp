#include "QtGraphicsSurface.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QSGRendererInterface>
#include <stdexcept>

namespace rnqnx::graphics {
namespace {

GraphicsBackend backendFor(QSGRendererInterface::GraphicsApi api) noexcept {
  switch (api) {
    case QSGRendererInterface::Software: return GraphicsBackend::Software;
    case QSGRendererInterface::OpenVG: return GraphicsBackend::OpenVG;
    case QSGRendererInterface::OpenGL: return GraphicsBackend::OpenGL;
    case QSGRendererInterface::Direct3D11: return GraphicsBackend::Direct3D11;
    case QSGRendererInterface::Vulkan: return GraphicsBackend::Vulkan;
    case QSGRendererInterface::Metal: return GraphicsBackend::Metal;
    case QSGRendererInterface::Null: return GraphicsBackend::Null;
    case QSGRendererInterface::Direct3D12: return GraphicsBackend::Direct3D12;
    default: return GraphicsBackend::Unknown;
  }
}

}  // namespace

QtGraphicsSurface::QtGraphicsSurface(QQuickWindow& window, QObject* parent)
    : QObject(parent), owner_(window.thread()), window_(&window) {
  requireOwner();
  if (!QGuiApplication::instance() || QGuiApplication::instance()->thread() != owner_) {
    throw std::logic_error("Graphics surface observation requires the GUI application owner.");
  }
  qRegisterMetaType<GraphicsSurfaceSnapshot>();
  qRegisterMetaType<GraphicsFrameSwap>();
  elapsed_.start();

  // These signals can originate on Qt's render thread. Explicit queuing keeps
  // all state and consumer callbacks on the GUI owner even with a basic loop.
  connections_.push_back(connect(&window, &QQuickWindow::sceneGraphInitialized,
      this, &QtGraphicsSurface::recordSceneGraphInitialized, Qt::QueuedConnection));
  connections_.push_back(connect(&window, &QQuickWindow::sceneGraphInvalidated,
      this, &QtGraphicsSurface::recordSceneGraphInvalidated, Qt::QueuedConnection));
  connections_.push_back(connect(&window, &QQuickWindow::frameSwapped,
      this, &QtGraphicsSurface::recordFrameSwap, Qt::QueuedConnection));
  connections_.push_back(connect(&window, &QWindow::widthChanged,
      this, &QtGraphicsSurface::refreshWindowState));
  connections_.push_back(connect(&window, &QWindow::heightChanged,
      this, &QtGraphicsSurface::refreshWindowState));
  connections_.push_back(connect(&window, &QWindow::visibleChanged,
      this, &QtGraphicsSurface::refreshWindowState));
  connections_.push_back(connect(&window, &QQuickWindow::devicePixelRatioChanged,
      this, &QtGraphicsSurface::refreshWindowState));
  connections_.push_back(connect(&window, &QObject::destroyed,
      this, &QtGraphicsSurface::recordWindowDestroyed));
  window.installEventFilter(this);

  snapshot_.viewport = GraphicsViewport::fromLogicalSize(window.size(), window.effectiveDevicePixelRatio());
  snapshot_.isVisible = window.isVisible();
  snapshot_.isExposed = window.isExposed();
  if (window.isSceneGraphInitialized()) {
    snapshot_.state = GraphicsSurfaceState::Initialized;
    snapshot_.backend = backendFor(window.rendererInterface()->graphicsApi());
    snapshot_.generation = 1;
  }
}

QtGraphicsSurface::~QtGraphicsSurface() {
  Q_ASSERT(QThread::currentThread() == owner_);
  disconnectWindow();
}

void QtGraphicsSurface::requireOwner() const {
  if (QThread::currentThread() != owner_ || thread() != owner_) {
    throw std::logic_error("Graphics surface observation requires its GUI owner thread.");
  }
}

bool QtGraphicsSurface::isTerminal() const noexcept {
  return snapshot_.state == GraphicsSurfaceState::Destroyed ||
         snapshot_.state == GraphicsSurfaceState::Closed;
}

GraphicsSurfaceSnapshot QtGraphicsSurface::snapshot() const {
  requireOwner();
  return snapshot_;
}

void QtGraphicsSurface::disconnectWindow() {
  for (const auto& connection : connections_) disconnect(connection);
  connections_.clear();
  if (window_) window_->removeEventFilter(this);
  window_.clear();
}

void QtGraphicsSurface::close() {
  requireOwner();
  if (snapshot_.state == GraphicsSurfaceState::Closed) return;
  finish(GraphicsSurfaceState::Closed);
}

void QtGraphicsSurface::finish(GraphicsSurfaceState state) {
  disconnectWindow();
  pending_.clear();
  snapshot_.state = state;
  snapshot_.backend = GraphicsBackend::Unknown;
  snapshot_.viewport = {};
  snapshot_.isVisible = false;
  snapshot_.isExposed = false;
  publishSnapshot(true);
}

void QtGraphicsSurface::recordWindowDestroyed() {
  requireOwner();
  if (!isTerminal()) finish(GraphicsSurfaceState::Destroyed);
}

void QtGraphicsSurface::refreshWindowState() {
  requireOwner();
  if (isTerminal() || !window_) return;
  auto* window = qobject_cast<QQuickWindow*>(window_.data());
  if (!window) return;
  const auto viewport = GraphicsViewport::fromLogicalSize(window->size(), window->effectiveDevicePixelRatio());
  const bool viewportChanged = snapshot_.viewport.logicalSize != viewport.logicalSize ||
      snapshot_.viewport.pixelSize != viewport.pixelSize ||
      snapshot_.viewport.devicePixelRatio != viewport.devicePixelRatio;
  const bool visible = window->isVisible();
  const bool exposed = window->isExposed();
  if (!viewportChanged && snapshot_.isVisible == visible && snapshot_.isExposed == exposed) return;
  snapshot_.viewport = viewport;
  snapshot_.isVisible = visible;
  snapshot_.isExposed = exposed;
  publishSnapshot(viewportChanged);
}

void QtGraphicsSurface::recordSceneGraphInitialized() {
  requireOwner();
  if (isTerminal() || !window_) return;
  auto* window = qobject_cast<QQuickWindow*>(window_.data());
  if (!window || !window->isSceneGraphInitialized()) return;
  snapshot_.state = GraphicsSurfaceState::Initialized;
  snapshot_.backend = backendFor(window->rendererInterface()->graphicsApi());
  publishSnapshot(true);
}

void QtGraphicsSurface::recordSceneGraphInvalidated() {
  requireOwner();
  if (isTerminal() || !window_) return;
  snapshot_.state = GraphicsSurfaceState::Invalidated;
  snapshot_.backend = GraphicsBackend::Unknown;
  publishSnapshot(true);
}

void QtGraphicsSurface::recordFrameSwap() {
  requireOwner();
  if (!window_ || snapshot_.state != GraphicsSurfaceState::Initialized) return;
  pending_.emplace_back(GraphicsFrameSwap{1, ++frameSequence_, snapshot_.generation, elapsed_.nsecsElapsed()});
  drainEvents();
}

bool QtGraphicsSurface::eventFilter(QObject* watched, QEvent* event) {
  if (watched != window_.data() || isTerminal()) return false;
  if (event->type() == QEvent::Expose || event->type() == QEvent::DevicePixelRatioChange) {
    refreshWindowState();
  } else if (event->type() == QEvent::PlatformSurface) {
    const auto* surfaceEvent = static_cast<QPlatformSurfaceEvent*>(event);
    if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
      snapshot_.state = GraphicsSurfaceState::Invalidated;
      snapshot_.backend = GraphicsBackend::Unknown;
    } else if (auto* window = qobject_cast<QQuickWindow*>(window_.data());
               window && window->isSceneGraphInitialized()) {
      snapshot_.state = GraphicsSurfaceState::Initialized;
      snapshot_.backend = backendFor(window->rendererInterface()->graphicsApi());
    }
    // Invalidate the observer's generation for both creation and destruction,
    // even if this platform preserves its scene graph across native surfaces.
    publishSnapshot(true);
  }
  return false;
}

void QtGraphicsSurface::publishSnapshot(bool invalidateGeneration) {
  if (invalidateGeneration) ++snapshot_.generation;
  ++snapshot_.sequence;
  pending_.emplace_back(snapshot_);
  drainEvents();
}

void QtGraphicsSurface::drainEvents() {
  if (draining_) return;
  draining_ = true;
  while (!pending_.empty()) {
    const auto observation = pending_.front();
    pending_.pop_front();
    const QPointer<QtGraphicsSurface> alive(this);
    if (const auto* snapshot = std::get_if<GraphicsSurfaceSnapshot>(&observation)) {
      Q_EMIT snapshotChanged(*snapshot);
    } else {
      Q_EMIT frameSwapped(std::get<GraphicsFrameSwap>(observation));
    }
    if (!alive) return;
  }
  draining_ = false;
}

}  // namespace rnqnx::graphics
