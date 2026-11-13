// Renders WebGPU frames on a dedicated thread that owns the Dawn device (and so
// its EGL context) and hands finished frames to the GUI thread as QImages.
// Keeping Dawn off the Qt render thread means Dawn never makes its own EGL
// context current underneath Qt's.
#pragma once

#include <QImage>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <memory>

#include "GpuContext.h"
#include "Scenarios.h"

class WebGpuFrameSource : public QObject {
  Q_OBJECT
 public:
  explicit WebGpuFrameSource(int width, int height, QObject* parent = nullptr);
  ~WebGpuFrameSource() override;

  // Starts the worker thread and creates the Dawn device on it. Blocks until
  // the device exists. Returns false with the error text on failure.
  bool start(QString* error);
  void stop();
  quint64 framesProduced() const { return frames_.loadRelaxed(); }
  // The GUI thread calls this once it has taken the latest frame. At most one
  // frame is in flight, so a stalled GUI thread cannot make the queue grow.
  void frameConsumed() { inFlight_.storeRelease(0); }

 Q_SIGNALS:
  void frameReady(QImage image);

 private Q_SLOTS:
  void renderFrame();

 private:
  const int width_;
  const int height_;
  QThread thread_;
  QTimer* timer_ = nullptr;
  std::unique_ptr<rnqnx::webgpu::GpuContext> gpu_;
  wgpu::Texture target_;
  rnqnx::webgpu::TriangleRenderer renderer_;
  QAtomicInteger<quint64> frames_{0};
  QAtomicInt inFlight_{0};
  quint64 tick_ = 0;
  QString startError_;
  bool started_ = false;
};
