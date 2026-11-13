#include "ScreenWindow.h"

#include "Log.h"

#if defined(__QNX__)

#include <EGL/egl.h>
#include <screen/screen.h>

namespace rnqnx::webgpu {

struct ScreenWindow::Impl {
  screen_context_t context = nullptr;
  screen_window_t window = nullptr;
  screen_event_t event = nullptr;
  int width = 0;
  int height = 0;
};

namespace {

// The window's pixel format has to match the EGLConfig Dawn picks for it
// (RGBA8, window-capable, ES3). Ask EGL which native visual that config uses.
int visualIdForEs3Window() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major = 0, minor = 0;
  if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) return -1;
  const EGLint attributes[] = {EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES3_BIT,  EGL_RED_SIZE,   8,
                               EGL_GREEN_SIZE,      8,              EGL_BLUE_SIZE,
                               8,                   EGL_ALPHA_SIZE, 8,
                               EGL_NONE};
  EGLConfig config;
  EGLint count = 0;
  if (!eglChooseConfig(display, attributes, &config, 1, &count) || count == 0) return -1;
  EGLint visual = -1;
  eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visual);
  return visual;
}

}  // namespace

ScreenWindow::ScreenWindow(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

std::unique_ptr<ScreenWindow> ScreenWindow::create(const Options& options, std::string& error) {
  auto impl = std::make_unique<Impl>();
  if (screen_create_context(&impl->context, SCREEN_APPLICATION_CONTEXT) != 0) {
    error = "screen_create_context failed";
    return nullptr;
  }
  auto fail = [&](const char* what) -> std::unique_ptr<ScreenWindow> {
    error = what;
    if (impl->event) screen_destroy_event(impl->event);
    if (impl->window) screen_destroy_window(impl->window);
    screen_destroy_context(impl->context);
    return nullptr;
  };
  if (screen_create_window(&impl->window, impl->context) != 0) {
    return fail("screen_create_window failed");
  }
  const int visual = visualIdForEs3Window();
  logLine("WINDOW", "egl_native_visual_id=%d", visual);
  int format = visual > 0 ? visual : SCREEN_FORMAT_RGBA8888;
  int usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_OPENGL_ES3;
  int size[2] = {options.width, options.height};
  int position[2] = {options.x, options.y};
  if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_FORMAT, &format) != 0 ||
      screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_USAGE, &usage) != 0 ||
      screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, size) != 0 ||
      screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_SIZE, size) != 0 ||
      screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_POSITION, position) != 0) {
    return fail("screen_set_window_property_iv failed");
  }
  screen_set_window_property_cv(impl->window, SCREEN_PROPERTY_ID_STRING,
                                static_cast<int>(options.idString.size()),
                                options.idString.c_str());
  // libEGL creates its surface on the window's render buffers and aborts the
  // process when there are none, so they have to exist first.
  if (screen_create_window_buffers(impl->window, 2) != 0) {
    return fail("screen_create_window_buffers failed");
  }
  if (screen_create_event(&impl->event) != 0) return fail("screen_create_event failed");
  impl->width = options.width;
  impl->height = options.height;
  return std::unique_ptr<ScreenWindow>(new ScreenWindow(std::move(impl)));
}

ScreenWindow::~ScreenWindow() {
  if (!impl_) return;
  if (impl_->event) screen_destroy_event(impl_->event);
  if (impl_->window) screen_destroy_window(impl_->window);
  if (impl_->context) screen_destroy_context(impl_->context);
}

void* ScreenWindow::nativeHandle() const { return impl_->window; }
int ScreenWindow::width() const { return impl_->width; }
int ScreenWindow::height() const { return impl_->height; }

bool ScreenWindow::resize(int width, int height, std::string& error) {
  int size[2] = {width, height};
  // New buffer dimensions need new buffers. The EGL surface on the old ones
  // has to be gone by now (SurfacePresenter::resize unconfigures first).
  screen_destroy_window_buffers(impl_->window);
  if (screen_set_window_property_iv(impl_->window, SCREEN_PROPERTY_SIZE, size) != 0 ||
      screen_set_window_property_iv(impl_->window, SCREEN_PROPERTY_BUFFER_SIZE, size) != 0 ||
      screen_create_window_buffers(impl_->window, 2) != 0) {
    error = "resizing the Screen window failed";
    return false;
  }
  impl_->width = width;
  impl_->height = height;
  return true;
}

bool ScreenWindow::querySizes(int windowSize[2], int bufferSize[2]) const {
  return screen_get_window_property_iv(impl_->window, SCREEN_PROPERTY_SIZE, windowSize) == 0 &&
         screen_get_window_property_iv(impl_->window, SCREEN_PROPERTY_BUFFER_SIZE, bufferSize) == 0;
}

bool ScreenWindow::pumpEvents() {
  bool keepOpen = true;
  while (screen_get_event(impl_->context, impl_->event, 0) == 0) {
    int type = SCREEN_EVENT_NONE;
    screen_get_event_property_iv(impl_->event, SCREEN_PROPERTY_TYPE, &type);
    if (type == SCREEN_EVENT_NONE) break;
    if (type == SCREEN_EVENT_CLOSE) keepOpen = false;
    if (type == SCREEN_EVENT_MANAGER) {
      int subtype = 0;
      screen_get_event_property_iv(impl_->event, SCREEN_PROPERTY_SUBTYPE, &subtype);
      if (subtype == SCREEN_EVENT_CLOSE) keepOpen = false;
    }
  }
  return keepOpen;
}

}  // namespace rnqnx::webgpu

#else  // !__QNX__

namespace rnqnx::webgpu {

struct ScreenWindow::Impl {};

ScreenWindow::ScreenWindow(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
std::unique_ptr<ScreenWindow> ScreenWindow::create(const Options&, std::string& error) {
  error = "QNX Screen is not available on this host";
  return nullptr;
}
ScreenWindow::~ScreenWindow() = default;
void* ScreenWindow::nativeHandle() const { return nullptr; }
int ScreenWindow::width() const { return 0; }
int ScreenWindow::height() const { return 0; }
bool ScreenWindow::resize(int, int, std::string& error) {
  error = "no window";
  return false;
}
bool ScreenWindow::querySizes(int[2], int[2]) const { return false; }
bool ScreenWindow::pumpEvents() { return true; }

}  // namespace rnqnx::webgpu

#endif
