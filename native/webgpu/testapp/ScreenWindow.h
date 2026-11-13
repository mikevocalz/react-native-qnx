// A QNX Screen application window that Dawn's OpenGL ES backend can present to.
// The test app owns its own Screen context, so it appears as one more window
// under rnqnx-winmgr (or any other window manager); it never touches another
// application's windows.
#pragma once

#include <memory>
#include <string>

namespace rnqnx::webgpu {

class ScreenWindow {
 public:
  struct Options {
    std::string idString = "org.rnqnx.webgpu-stage5";
    int x = 120;
    int y = 90;
    int width = 640;
    int height = 480;
  };

  // Returns null (with `error`) when Screen or EGL is unavailable, which is the
  // case on every non-QNX host.
  static std::unique_ptr<ScreenWindow> create(const Options& options, std::string& error);
  ~ScreenWindow();

  ScreenWindow(const ScreenWindow&) = delete;
  ScreenWindow& operator=(const ScreenWindow&) = delete;

  // The screen_window_t, as the opaque pointer WebGPU's surface source carries.
  void* nativeHandle() const;
  int width() const;
  int height() const;

  // Changes the window size and its buffer size. A new EGL/WebGPU surface is
  // needed afterwards (callers unconfigure first).
  bool resize(int width, int height, std::string& error);

  // Reads the size Screen reports for the window and for its buffers (they can
  // differ from what was last requested). Returns false if the query failed.
  bool querySizes(int windowSize[2], int bufferSize[2]) const;

  // Drains pending Screen events. Returns false once the window manager asked
  // the window to close.
  bool pumpEvents();

 private:
  struct Impl;
  explicit ScreenWindow(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace rnqnx::webgpu
