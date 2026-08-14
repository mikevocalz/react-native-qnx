#pragma once

#include <screen/screen.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace rnqnx {

/**
 * Minimal RAII wrapper over a QNX Screen window and its render buffers.
 *
 * Owns one screen_context_t and one screen_window_t. Creates a double-buffered
 * window suitable for Screen native fill operations. Does not depend on process
 * termination for cleanup.
 *
 * The API calls and constants used here follow the QNX SDP 8.0 Screen reference.
 * This translation unit still requires compilation against the licensed SDP's
 * installed screen/screen.h before it can be considered QNX-validated.
 */
class QnxScreenSurface {
 public:
  static constexpr int kBufferCount = 2;

  QnxScreenSurface() = default;

  /**
   * Create the Screen context and window.
   *
   * @param windowType one of the QNX Screen window types
   * @param width initial window and render-buffer width in pixels
   * @param height initial window and render-buffer height in pixels
   * @throws std::runtime_error on any Screen failure
   * @throws std::invalid_argument when width or height is not positive
   * @throws std::logic_error when the surface is already initialized
   */
  void initialize(
      int windowType = SCREEN_APPLICATION_WINDOW,
      int width = 1280,
      int height = 720);

  /** Release all Screen resources deterministically. */
  void destroy() noexcept;

  ~QnxScreenSurface() noexcept { destroy(); }

  // Non-copyable; ownership is tied to the underlying native handles.
  QnxScreenSurface(const QnxScreenSurface&) = delete;
  QnxScreenSurface& operator=(const QnxScreenSurface&) = delete;

  QnxScreenSurface(QnxScreenSurface&& other) noexcept;
  QnxScreenSurface& operator=(QnxScreenSurface&& other) noexcept;

  /**
   * Return the first buffer currently available for rendering.
   *
   * Screen may rotate SCREEN_PROPERTY_RENDER_BUFFERS after every post, so this
   * method queries the property each time instead of cycling stale handles.
   */
  screen_buffer_t currentBuffer();

  /**
   * Present the current buffer via screen_post_window().
   *
   * If posting fails, the successfully rendered buffer remains pending so the
   * caller can retry the post or destroy the surface.
   */
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
  screen_buffer_t pendingBuffer_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  bool initialized_ = false;

  static void throwOnError(int code, const std::string& message);
};

} // namespace rnqnx
