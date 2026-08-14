#include "QnxScreenSurface.h"

#include <array>
#include <bit>
#include <cerrno>
#include <screen/screen.h>
#include <system_error>

namespace rnqnx {

void QnxScreenSurface::throwOnError(int code, const std::string& message) {
  if (code != 0) {
    const int error = errno == 0 ? EIO : errno;
    throw std::system_error(error, std::generic_category(), message);
  }
}

void QnxScreenSurface::initialize(int windowType, int width, int height) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("QnxScreenSurface dimensions must be positive");
  }
  if (initialized_) {
    throw std::logic_error("QnxScreenSurface is already initialized");
  }

  try {
    int rc = screen_create_context(&context_, SCREEN_APPLICATION_CONTEXT);
    throwOnError(rc, "screen_create_context failed");

    rc = screen_create_window_type(&window_, context_, windowType);
    throwOnError(rc, "screen_create_window_type failed");

    int usage = SCREEN_USAGE_NATIVE;
    rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_USAGE, &usage);
    throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_USAGE) failed");

    int format = SCREEN_FORMAT_RGBA8888;
    rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_FORMAT, &format);
    throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT) failed");

    int size[2] = {width, height};
    rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_BUFFER_SIZE, size);
    throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_BUFFER_SIZE) failed");
    rc = screen_set_window_property_iv(window_, SCREEN_PROPERTY_SIZE, size);
    throwOnError(rc, "screen_set_window_property_iv(SCREEN_PROPERTY_SIZE) failed");

    rc = screen_create_window_buffers(window_, kBufferCount);
    throwOnError(rc, "screen_create_window_buffers failed");

    int actualCount = 0;
    rc = screen_get_window_property_iv(
        window_, SCREEN_PROPERTY_RENDER_BUFFER_COUNT, &actualCount);
    throwOnError(
        rc, "screen_get_window_property_iv(SCREEN_PROPERTY_RENDER_BUFFER_COUNT) failed");
    if (actualCount != kBufferCount) {
      throw std::runtime_error(
          "Unexpected buffer count: requested " + std::to_string(kBufferCount) +
          ", got " + std::to_string(actualCount));
    }

    pendingBuffer_ = nullptr;
    width_ = width;
    height_ = height;
    initialized_ = true;
  } catch (...) {
    destroy();
    throw;
  }
}

void QnxScreenSurface::destroy() noexcept {
  if (window_) {
    screen_destroy_window(window_);
    window_ = nullptr;
  }
  if (context_) {
    screen_destroy_context(context_);
    context_ = nullptr;
  }

  pendingBuffer_ = nullptr;
  width_ = 0;
  height_ = 0;
  initialized_ = false;
}

QnxScreenSurface::QnxScreenSurface(QnxScreenSurface&& other) noexcept
    : context_(other.context_),
      window_(other.window_),
      pendingBuffer_(other.pendingBuffer_),
      width_(other.width_),
      height_(other.height_),
      initialized_(other.initialized_) {
  other.context_ = nullptr;
  other.window_ = nullptr;
  other.pendingBuffer_ = nullptr;
  other.width_ = 0;
  other.height_ = 0;
  other.initialized_ = false;
}

QnxScreenSurface& QnxScreenSurface::operator=(QnxScreenSurface&& other) noexcept {
  if (this != &other) {
    destroy();
    context_ = other.context_;
    window_ = other.window_;
    pendingBuffer_ = other.pendingBuffer_;
    width_ = other.width_;
    height_ = other.height_;
    initialized_ = other.initialized_;

    other.context_ = nullptr;
    other.window_ = nullptr;
    other.pendingBuffer_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
    other.initialized_ = false;
  }
  return *this;
}

screen_buffer_t QnxScreenSurface::currentBuffer() {
  if (!initialized_) {
    throw std::runtime_error("QnxScreenSurface::currentBuffer called before initialize()");
  }

  std::array<screen_buffer_t, kBufferCount> buffers{};
  const int rc = screen_get_window_property_pv(
      window_, SCREEN_PROPERTY_RENDER_BUFFERS, reinterpret_cast<void**>(buffers.data()));
  throwOnError(rc, "screen_get_window_property_pv(SCREEN_PROPERTY_RENDER_BUFFERS) failed");
  if (buffers.front() == nullptr) {
    throw std::runtime_error("Screen returned a null render buffer");
  }
  pendingBuffer_ = buffers.front();
  return buffers.front();
}

void QnxScreenSurface::present() {
  if (!initialized_) {
    throw std::runtime_error("QnxScreenSurface::present called before initialize()");
  }

  const screen_buffer_t buffer = pendingBuffer_ == nullptr ? currentBuffer() : pendingBuffer_;
  const int rc = screen_post_window(window_, buffer, 0, nullptr, 0);
  throwOnError(rc, "screen_post_window failed");
  pendingBuffer_ = nullptr;
}

void QnxScreenSurface::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (!initialized_) {
    throw std::runtime_error("QnxScreenSurface::fill called before initialize()");
  }

  static_assert(sizeof(int) == sizeof(std::uint32_t));
  const std::uint32_t argb =
      (static_cast<std::uint32_t>(a) << 24U) |
      (static_cast<std::uint32_t>(r) << 16U) |
      (static_cast<std::uint32_t>(g) << 8U) |
      static_cast<std::uint32_t>(b);
  const int color = std::bit_cast<std::int32_t>(argb);
  const int attributes[] = {
      SCREEN_BLIT_COLOR,
      color,
      SCREEN_BLIT_DESTINATION_X,
      0,
      SCREEN_BLIT_DESTINATION_Y,
      0,
      SCREEN_BLIT_DESTINATION_WIDTH,
      width_,
      SCREEN_BLIT_DESTINATION_HEIGHT,
      height_,
      SCREEN_BLIT_END,
  };

  const screen_buffer_t buffer = currentBuffer();
  const int rc = screen_fill(context_, buffer, attributes);
  if (rc != 0) {
    pendingBuffer_ = nullptr;
    throwOnError(rc, "screen_fill failed");
  }
}

} // namespace rnqnx
