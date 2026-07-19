#include "QnxScreenSurface.h"

#include <screen/screen.h>
#include <cstring>

namespace rnqnx {

void QnxScreenSurface::throwOnError(int code, const std::string& message) {
  if (code != 0) {
    throw std::runtime_error(message + ": screen error " + std::to_string(code));
  }
}

void QnxScreenSurface::initialize(int windowType) {
  if (initialized_) {
    return;
  }

  int rc = screen_create_context(&context_, SCREEN_APPLICATION_CONTEXT);
  throwOnError(rc, "screen_create_context failed");

  rc = screen_create_window(&window_, context_);
  throwOnError(rc, "screen_create_window failed");

  rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_TYPE, &windowType);
  throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_TYPE) failed");

  int usage = SCREEN_USAGE_NATIVE;
  rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_USAGE, &usage);
  throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_USAGE) failed");

  int format = SCREEN_FORMAT_RGBA8888;
  rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_FORMAT, &format);
  throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT) failed");

  int bufferCount = kBufferCount;
  rc = screen_create_window_buffers(window_, bufferCount);
  throwOnError(rc, "screen_create_window_buffers failed");

  rc = screen_get_window_property_pv(
      window_, SCREEN_PROPERTY_RENDER_BUFFERS, reinterpret_cast<void**>(buffers_.data()));
  throwOnError(rc, "screen_get_window_property_pv(SCREEN_PROPERTY_RENDER_BUFFERS) failed");

  int actualCount = 0;
  rc = screen_get_window_property_iv(window_, SCREEN_PROPERTY_RENDER_BUFFER_COUNT, &actualCount);
  throwOnError(rc, "screen_get_window_property_iv(SCREEN_PROPERTY_RENDER_BUFFER_COUNT) failed");
  if (actualCount != kBufferCount) {
    throw std::runtime_error(
        "Unexpected buffer count: requested " + std::to_string(kBufferCount) +
        ", got " + std::to_string(actualCount));
  }

  // Default size; can be overridden with setSize().
  setSize(1280, 720);

  bufferIndex_ = 0;
  initialized_ = true;
}

void QnxScreenSurface::destroy() noexcept {
  if (!initialized_) {
    return;
  }

  if (window_) {
    screen_destroy_window(window_);
    window_ = nullptr;
  }
  if (context_) {
    screen_destroy_context(context_);
    context_ = nullptr;
  }

  buffers_.fill(nullptr);
  bufferIndex_ = 0;
  width_ = 0;
  height_ = 0;
  initialized_ = false;
}

QnxScreenSurface::QnxScreenSurface(QnxScreenSurface&& other) noexcept
    : context_(other.context_),
      window_(other.window_),
      buffers_(other.buffers_),
      bufferIndex_(other.bufferIndex_),
      width_(other.width_),
      height_(other.height_),
      initialized_(other.initialized_) {
  other.context_ = nullptr;
  other.window_ = nullptr;
  other.buffers_.fill(nullptr);
  other.bufferIndex_ = 0;
  other.width_ = 0;
  other.height_ = 0;
  other.initialized_ = false;
}

QnxScreenSurface& QnxScreenSurface::operator=(QnxScreenSurface&& other) noexcept {
  if (this != &other) {
    destroy();
    context_ = other.context_;
    window_ = other.window_;
    buffers_ = other.buffers_;
    bufferIndex_ = other.bufferIndex_;
    width_ = other.width_;
    height_ = other.height_;
    initialized_ = other.initialized_;

    other.context_ = nullptr;
    other.window_ = nullptr;
    other.buffers_.fill(nullptr);
    other.bufferIndex_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.initialized_ = false;
  }
  return *this;
}

void QnxScreenSurface::setSize(int width, int height) {
  if (!initialized_) {
    throw std::runtime_error("QnxScreenSurface::setSize called before initialize()");
  }

  int size[2] = {width, height};
  int rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_SIZE, size);
  throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_SIZE) failed");

  width_ = width;
  height_ = height;
}

void QnxScreenSurface::present() {
  if (!initialized_) {
    throw std::runtime_error("QnxScreenSurface::present called before initialize()");
  }

  int rc = screen_post_window(window_, currentBuffer(), 0, nullptr, 0);
  throwOnError(rc, "screen_post_window failed");

  bufferIndex_ = (bufferIndex_ + 1) % kBufferCount;
}

void QnxScreenSurface::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (!initialized_) {
    throw std::runtime_error("QnxScreenSurface::fill called before initialize()");
  }

  int color = (static_cast<int>(a) << 24) |
              (static_cast<int>(b) << 16) |
              (static_cast<int>(g) << 8)  |
              static_cast<int>(r);

  int rect[4] = {0, 0, width_, height_};
  int rc = screen_fill(context_, currentBuffer(), rect);
  throwOnError(rc, "screen_fill failed");

  rc = screen_set_context_property_iv(context_, SCREEN_PROPERTY_COLOR, &color);
  throwOnError(rc, "screen_set_context_property_iv(SCREEN_PROPERTY_COLOR) failed");

  rc = screen_blit(context_, currentBuffer(), buffers_[0], rect);
  if (rc != 0) {
    // screen_blit may not be needed for a simple fill; ignore errors in this demo path.
  }
}

} // namespace rnqnx
