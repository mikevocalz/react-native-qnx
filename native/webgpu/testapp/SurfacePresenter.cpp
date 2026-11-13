#include "SurfacePresenter.h"

#include "Log.h"

namespace rnqnx::webgpu {

bool SurfacePresenter::create(std::string& error) {
  // On QNX, Dawn reads the screen_window_t from the Android-native-window
  // surface source (see patches/webgpu/dawn/0002-qnx-screen-window-surface.patch).
  wgpu::SurfaceSourceAndroidNativeWindow source{};
  source.window = window_.nativeHandle();
  wgpu::SurfaceDescriptor descriptor{};
  descriptor.nextInChain = &source;
  surface_ = gpu_.instance().CreateSurface(&descriptor);
  if (!surface_) {
    error = "CreateSurface returned null";
    return false;
  }
  return configure(static_cast<uint32_t>(window_.width()), static_cast<uint32_t>(window_.height()),
                   error);
}

bool SurfacePresenter::configure(uint32_t width, uint32_t height, std::string& error) {
  wgpu::SurfaceCapabilities capabilities{};
  surface_.GetCapabilities(gpu_.adapter(), &capabilities);
  if (capabilities.formatCount == 0 || capabilities.presentModeCount == 0) {
    error = "surface reports no formats or present modes";
    return false;
  }
  // Pick a plain 8-bit UNORM format: an sRGB format would make the colour
  // checks depend on the transfer function. Fall back to the first offered.
  format_ = capabilities.formats[0];
  for (size_t i = 0; i < capabilities.formatCount; ++i) {
    if (capabilities.formats[i] == wgpu::TextureFormat::RGBA8Unorm ||
        capabilities.formats[i] == wgpu::TextureFormat::BGRA8Unorm) {
      format_ = capabilities.formats[i];
      break;
    }
  }
  if ((static_cast<uint64_t>(capabilities.usages) &
       static_cast<uint64_t>(wgpu::TextureUsage::CopySrc)) == 0) {
    error = "surface textures cannot be a copy source, so frames cannot be read back";
    return false;
  }
  logLine("SURFACE", "capabilities formats=%zu first_format=%u present_modes=%zu usages=0x%llx",
          capabilities.formatCount, static_cast<uint32_t>(format_), capabilities.presentModeCount,
          (unsigned long long)capabilities.usages);

  wgpu::SurfaceConfiguration configuration{};
  configuration.device = gpu_.device();
  configuration.format = format_;
  configuration.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
  configuration.width = width;
  configuration.height = height;
  configuration.presentMode = wgpu::PresentMode::Fifo;
  surface_.Configure(&configuration);
  width_ = width;
  height_ = height;
  logLine("SURFACE", "configured %ux%u", width, height);
  return renderer_.init(gpu_.device(), format_, error);
}

bool SurfacePresenter::resize(int width, int height, std::string& error) {
  // The QNX EGL window surface does not follow a window resize, so the old
  // surface is released before the window changes size.
  surface_.Unconfigure();
  if (!window_.resize(width, height, error)) return false;
  return configure(static_cast<uint32_t>(width), static_cast<uint32_t>(height), error);
}

void SurfacePresenter::dropSurface() {
  if (surface_) surface_.Unconfigure();
  surface_ = nullptr;
}

void SurfacePresenter::releaseDeviceObjects() {
  dropSurface();
  renderer_.reset();
}

bool SurfacePresenter::drawFrame(wgpu::Color clear, bool triangle, Image& pixels,
                                 std::string& error) {
  wgpu::SurfaceTexture current{};
  surface_.GetCurrentTexture(&current);
  if (current.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
      current.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
    error = "GetCurrentTexture status " + std::to_string(static_cast<int>(current.status));
    return false;
  }
  renderer_.encode(gpu_.device(), gpu_.queue(), current.texture.CreateView(), clear, triangle);

  const bool bgra = format_ == wgpu::TextureFormat::BGRA8Unorm ||
                    format_ == wgpu::TextureFormat::BGRA8UnormSrgb;
  // Read back the dimensions of the texture Dawn actually handed out.
  if (!readbackTexture(gpu_, current.texture, current.texture.GetWidth(),
                       current.texture.GetHeight(), bgra, pixels, error)) {
    return false;
  }
  if (surface_.Present() != wgpu::Status::Success) {
    error = "Surface::Present failed";
    return false;
  }
  return true;
}

}  // namespace rnqnx::webgpu
