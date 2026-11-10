#pragma once

#include "GraphicsSurfaceState.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QThread>
#include <deque>
#include <variant>
#include <vector>

namespace rnqnx::graphics {

// Observes one existing Qt Quick window without allocating or taking ownership
// of its GPU resources. Construct, read, close and destroy on the GUI owner.
// Connect first, then read snapshot() on that thread for initial state.
// close() is idempotent; destruction of the window leaves a terminal Destroyed
// snapshot. Create another observer for another window.
class QtGraphicsSurface final : public QObject {
  Q_OBJECT

 public:
  explicit QtGraphicsSurface(QQuickWindow& window, QObject* parent = nullptr);
  ~QtGraphicsSurface() override;

  GraphicsSurfaceSnapshot snapshot() const;
  void close();

 Q_SIGNALS:
  void snapshotChanged(rnqnx::graphics::GraphicsSurfaceSnapshot snapshot);
  // A queued Qt frameSwapped observation. Emitted on the GUI owner only, and
  // only while the latest delivered scene-graph state is Initialized. This
  // cannot schedule work inside the render thread's current frame.
  void frameSwapped(rnqnx::graphics::GraphicsFrameSwap frame);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void requireOwner() const;
  void refreshWindowState();
  void recordSceneGraphInitialized();
  void recordSceneGraphInvalidated();
  void recordFrameSwap();
  void recordWindowDestroyed();
  void finish(GraphicsSurfaceState state);
  void disconnectWindow();
  void publishSnapshot(bool invalidateGeneration);
  void drainEvents();
  bool isTerminal() const noexcept;

  using Observation = std::variant<GraphicsSurfaceSnapshot, GraphicsFrameSwap>;
  QThread* const owner_;
  // QObject guard remains valid during base-class window destruction. A
  // QPointer<QQuickWindow> would downcast after the QQuickWindow part is gone.
  QPointer<QObject> window_;
  GraphicsSurfaceSnapshot snapshot_;
  QElapsedTimer elapsed_;
  quint64 frameSequence_{0};
  std::vector<QMetaObject::Connection> connections_;
  std::deque<Observation> pending_;
  bool draining_{false};
};

}  // namespace rnqnx::graphics
