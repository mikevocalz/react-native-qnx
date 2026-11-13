// Presents WebGPU frames to a ScreenWindow through a wgpu::Surface. It keeps
// the surface lifecycle explicit: configure, resize, drop and recreate.
#pragma once

#include <dawn/webgpu_cpp.h>

#include <string>

#include "GpuContext.h"
#include "Scenarios.h"
#include "ScreenWindow.h"

namespace rnqnx::webgpu {

class SurfacePresenter {
 public:
  SurfacePresenter(GpuContext& gpu, ScreenWindow& window) : gpu_(gpu), window_(window) {}

  // Creates the wgpu::Surface from the window and configures it at the
  // window's current size. Rebuilds the triangle pipeline for the format.
  bool create(std::string& error);
  // Resizes the window, then unconfigures and reconfigures the surface.
  bool resize(int width, int height, std::string& error);
  // Drops the surface (and its EGL surface) without touching the window.
  void dropSurface();
  // Drops everything tied to the current device (surface and pipeline). Call it
  // before GpuContext::recreateDevice(); create() makes them again.
  void releaseDeviceObjects();
  bool valid() const { return static_cast<bool>(surface_); }

  // Renders one frame, reads the surface texture back before presenting and
  // returns it in `pixels`. Returns false on any failure.
  bool drawFrame(wgpu::Color clear, bool triangle, Image& pixels, std::string& error);

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }
  wgpu::TextureFormat format() const { return format_; }

 private:
  bool configure(uint32_t width, uint32_t height, std::string& error);

  GpuContext& gpu_;
  ScreenWindow& window_;
  wgpu::Surface surface_;
  wgpu::TextureFormat format_ = wgpu::TextureFormat::Undefined;
  TriangleRenderer renderer_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

}  // namespace rnqnx::webgpu
