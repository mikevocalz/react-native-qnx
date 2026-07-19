#pragma once

#include <screen/screen.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace rnqnx {

/**
 * Minimal RAII wrapper over a QNX Screen top-level window and its render buffers.
 *
 * Owns one screen_context_t and one screen_window_t. Creates a double-buffered
 * window suitable for GPU/CPU rendering. Does not depend on process termination
 * for cleanup.
 */
class QnxScreenSurface {
 public:
  static constexpr int kBufferCount = 2;

  QnxScreenSurface() = default;

  /**
   * Create the Screen context and window.
   *
   * @param windowType SCREEN_WINDOW_TYPE_NORMAL or SCREEN_WINDOW_TYPE_APPLICATION
   * @throws std::runtime_error on any Screen failure
   */
  void initialize(int windowType = SCREEN_WINDOW_TYPE_APPLICATION);

  /** Release all Screen resources deterministically. */
  void destroy() noexcept;

  ~QnxScreenSurface() noexcept { destroy(); }

  // Non-copyable; ownership is tied to the underlying native handles.
  QnxScreenSurface(const QnxScreenSurface&) = delete;
  QnxScreenSurface& operator=(const QnxScreenSurface&) = delete;

  QnxScreenSurface(QnxScreenSurface&& other) noexcept;
  QnxScreenSurface& operator=(QnxScreenSurface&& other) noexcept;

  /** Resize the window in screen pixels. */
  void setSize(int width, int height);

  /** Return the current render buffer to draw into. */
  screen_buffer_t currentBuffer() const noexcept { return buffers_[bufferIndex_]; }

  /** Present the current buffer via screen_post_window(). */
  void present();

  /** Fill the current buffer with a solid RGBA color. */
  void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

  screen_context_t context() const noexcept { return context_; }
  screen_window_t window() const noexcept { return window_; }
  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }

 private:
  screen_context_t context_ = nullptr;
  screen_window_t window_ = nullptr;
  std::array<screen_buffer_t, kBufferCount> buffers_{nullptr, nullptr};
  int bufferIndex_ = 0;
  int width_ = 0;
  int height_ = 0;
  bool initialized_ = false;

  void throwOnError(int code, const std::string& message);
};

} // namespace rnqnx
