#include "WebGpuFrameSource.h"

#include <QMetaObject>

#include "Log.h"

using namespace rnqnx::webgpu;

WebGpuFrameSource::WebGpuFrameSource(int width, int height, QObject* parent)
    : QObject(parent), width_(width), height_(height) {}

WebGpuFrameSource::~WebGpuFrameSource() { stop(); }

bool WebGpuFrameSource::start(QString* error) {
  moveToThread(&thread_);
  thread_.start();
  // Device creation happens on the worker thread so its EGL context is
  // current only there.
  QMetaObject::invokeMethod(
      this,
      [this]() {
        std::string message;
        gpu_ = GpuContext::create(BackendChoice::OpenGLES, message);
        if (!gpu_) {
          startError_ = QString::fromStdString(message);
          return;
        }
        gpu_->logCapabilities();
        wgpu::TextureDescriptor descriptor{};
        descriptor.size = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1};
        descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        target_ = gpu_->device().CreateTexture(&descriptor);
        if (!renderer_.init(gpu_->device(), descriptor.format, message)) {
          startError_ = QString::fromStdString(message);
          gpu_.reset();
          return;
        }
        timer_ = new QTimer(this);
        timer_->setInterval(33);
        connect(timer_, &QTimer::timeout, this, &WebGpuFrameSource::renderFrame);
        timer_->start();
        started_ = true;
      },
      Qt::BlockingQueuedConnection);
  if (!started_) {
    if (error) *error = startError_;
    stop();
    return false;
  }
  return true;
}

void WebGpuFrameSource::stop() {
  if (!thread_.isRunning()) return;
  // Everything that owns Dawn/GL objects is released on the worker thread,
  // which is the one the EGL context is current on. Qt objects that live on
  // the worker go back to the GUI thread before it exits.
  QThread* const gui = QThread::currentThread();
  QMetaObject::invokeMethod(
      this,
      [this, gui]() {
        delete timer_;
        timer_ = nullptr;
        renderer_.reset();
        target_ = nullptr;
        gpu_.reset();
        moveToThread(gui);
      },
      Qt::BlockingQueuedConnection);
  thread_.quit();
  thread_.wait();
}

void WebGpuFrameSource::renderFrame() {
  const double phase = static_cast<double>(tick_++ % 90) / 90.0;
  renderer_.encode(gpu_->device(), gpu_->queue(), target_.CreateView(),
                   {0.05, 0.1 + 0.3 * phase, 0.4, 1.0}, true);
  Image image;
  std::string error;
  if (!readbackTexture(*gpu_, target_, static_cast<uint32_t>(width_),
                       static_cast<uint32_t>(height_), false, image, error)) {
    logLine("COMPOSE", "readback failed: %s", error.c_str());
    return;
  }
  QImage frame(image.pixels.data(), width_, height_, width_ * 4, QImage::Format_RGBA8888);
  frames_.fetchAndAddRelaxed(1);
  Q_EMIT frameReady(frame.copy());
}
