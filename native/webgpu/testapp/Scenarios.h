// GPU workloads for the Stage 5 test: clear, triangle, compute and readback.
// They only use the public WebGPU API, so the same code runs against the
// OpenGL ES backend on the Pi and the null backend on a CI host.
#pragma once

#include <dawn/webgpu_cpp.h>

#include <cstdint>
#include <string>
#include <vector>

#include "GpuContext.h"

namespace rnqnx::webgpu {

struct Rgba8 {
  uint8_t r, g, b, a;
};

// Tightly packed RGBA8 (or BGRA8) pixels read back from a texture.
struct Image {
  uint32_t width = 0;
  uint32_t height = 0;
  bool bgra = false;
  std::vector<uint8_t> pixels;  // width * height * 4

  // Returns the pixel as RGBA regardless of the stored channel order.
  Rgba8 at(uint32_t x, uint32_t y) const;
};

// Copies `texture` (2D, 4 bytes per texel) into a mappable buffer and reads it.
// The texture needs CopySrc usage. Returns false and fills `error` on failure.
bool readbackTexture(const GpuContext& gpu, const wgpu::Texture& texture, uint32_t width,
                     uint32_t height, bool bgra, Image& out, std::string& error);

// A pipeline that draws one centered triangle (red) into a target of `format`.
class TriangleRenderer {
 public:
  bool init(const wgpu::Device& device, wgpu::TextureFormat format, std::string& error);
  // Clears to `clear` and draws the triangle when `drawTriangle` is set.
  void encode(const wgpu::Device& device, const wgpu::Queue& queue, const wgpu::TextureView& view,
              wgpu::Color clear, bool drawTriangle) const;
  // Drops the pipeline. Call before the device it came from is replaced.
  void reset() { pipeline_ = nullptr; }

 private:
  wgpu::RenderPipeline pipeline_;
};

// Doubles `count` u32 values on the GPU (x * 2 + 1) and reads them back. With
// `verify` set, every value is compared; without it (the null backend executes
// nothing) only pipeline creation, dispatch, copy and map have to succeed.
bool runComputeDouble(const GpuContext& gpu, uint32_t count, bool verify, std::string& error);

}  // namespace rnqnx::webgpu
