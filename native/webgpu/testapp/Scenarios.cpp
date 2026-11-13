#include "Scenarios.h"

#include <cstring>
#include <memory>

namespace rnqnx::webgpu {

Rgba8 Image::at(uint32_t x, uint32_t y) const {
  const uint8_t* p = &pixels[(static_cast<size_t>(y) * width + x) * 4];
  return bgra ? Rgba8{p[2], p[1], p[0], p[3]} : Rgba8{p[0], p[1], p[2], p[3]};
}

namespace {

std::string toString(wgpu::StringView view) {
  if (view.data == nullptr) return {};
  if (view.length == WGPU_STRLEN) return std::string(view.data);
  return std::string(view.data, view.length);
}

constexpr uint32_t kRowAlignment = 256;  // WebGPU bytesPerRow rule for buffer copies

wgpu::ShaderModule makeShader(const wgpu::Device& device, const char* wgsl) {
  wgpu::ShaderSourceWGSL source{};
  source.code = wgpu::StringView(wgsl);
  wgpu::ShaderModuleDescriptor descriptor{};
  descriptor.nextInChain = &source;
  return device.CreateShaderModule(&descriptor);
}

bool mapAndRead(const GpuContext& gpu, const wgpu::Buffer& buffer, uint64_t size,
                std::vector<uint8_t>& bytes, std::string& error) {
  // The callback owns its result so a timed-out wait cannot leave it writing
  // into this function's dead stack.
  struct MapResult {
    bool done = false;
    bool mapped = false;
    std::string error;
  };
  auto result = std::make_shared<MapResult>();
  auto future = buffer.MapAsync(wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::WaitAnyOnly,
                                [result](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                                  result->done = true;
                                  result->mapped = status == wgpu::MapAsyncStatus::Success;
                                  if (!result->mapped) result->error = toString(message);
                                });
  if (!gpu.wait(future) || !result->done) {
    error = "buffer map timed out";
    return false;
  }
  if (!result->mapped) {
    error = "buffer map failed: " + result->error;
    return false;
  }
  const void* data = buffer.GetConstMappedRange(0, size);
  if (data == nullptr) {
    error = "GetConstMappedRange returned null";
    buffer.Unmap();
    return false;
  }
  bytes.resize(size);
  std::memcpy(bytes.data(), data, size);
  buffer.Unmap();
  return true;
}

}  // namespace

bool readbackTexture(const GpuContext& gpu, const wgpu::Texture& texture, uint32_t width,
                     uint32_t height, bool bgra, Image& out, std::string& error) {
  const wgpu::Device& device = gpu.device();
  const uint32_t unpadded = width * 4;
  const uint32_t stride = (unpadded + kRowAlignment - 1) / kRowAlignment * kRowAlignment;
  const uint64_t size = static_cast<uint64_t>(stride) * height;

  wgpu::BufferDescriptor bufferDescriptor{};
  bufferDescriptor.size = size;
  bufferDescriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer staging = device.CreateBuffer(&bufferDescriptor);

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  wgpu::TexelCopyTextureInfo source{};
  source.texture = texture;
  wgpu::TexelCopyBufferInfo destination{};
  destination.buffer = staging;
  destination.layout.bytesPerRow = stride;
  destination.layout.rowsPerImage = height;
  wgpu::Extent3D extent{width, height, 1};
  encoder.CopyTextureToBuffer(&source, &destination, &extent);
  wgpu::CommandBuffer commands = encoder.Finish();
  gpu.queue().Submit(1, &commands);

  std::vector<uint8_t> padded;
  if (!mapAndRead(gpu, staging, size, padded, error)) return false;

  out.width = width;
  out.height = height;
  out.bgra = bgra;
  out.pixels.resize(static_cast<size_t>(unpadded) * height);
  for (uint32_t row = 0; row < height; ++row) {
    std::memcpy(&out.pixels[static_cast<size_t>(row) * unpadded],
                &padded[static_cast<size_t>(row) * stride], unpadded);
  }
  return true;
}

bool TriangleRenderer::init(const wgpu::Device& device, wgpu::TextureFormat format,
                            std::string& error) {
  static const char* kShader = R"(
    @vertex fn vs(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
      var p = array<vec2f, 3>(vec2f(0.0, 0.8), vec2f(-0.8, -0.8), vec2f(0.8, -0.8));
      return vec4f(p[i], 0.0, 1.0);
    }
    @fragment fn fs() -> @location(0) vec4f { return vec4f(1.0, 0.0, 0.0, 1.0); }
  )";
  wgpu::ShaderModule module = makeShader(device, kShader);
  wgpu::ColorTargetState target{};
  target.format = format;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs";
  fragment.targetCount = 1;
  fragment.targets = &target;
  wgpu::RenderPipelineDescriptor descriptor{};
  descriptor.vertex.module = module;
  descriptor.vertex.entryPoint = "vs";
  descriptor.fragment = &fragment;
  pipeline_ = device.CreateRenderPipeline(&descriptor);
  if (!pipeline_) {
    error = "CreateRenderPipeline returned null";
    return false;
  }
  return true;
}

void TriangleRenderer::encode(const wgpu::Device& device, const wgpu::Queue& queue,
                              const wgpu::TextureView& view, wgpu::Color clear,
                              bool drawTriangle) const {
  wgpu::RenderPassColorAttachment attachment{};
  attachment.view = view;
  attachment.loadOp = wgpu::LoadOp::Clear;
  attachment.storeOp = wgpu::StoreOp::Store;
  attachment.clearValue = clear;
  wgpu::RenderPassDescriptor pass{};
  pass.colorAttachmentCount = 1;
  pass.colorAttachments = &attachment;

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  wgpu::RenderPassEncoder render = encoder.BeginRenderPass(&pass);
  if (drawTriangle) {
    render.SetPipeline(pipeline_);
    render.Draw(3);
  }
  render.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  queue.Submit(1, &commands);
}

bool runComputeDouble(const GpuContext& gpu, uint32_t count, bool verify, std::string& error) {
  const wgpu::Device& device = gpu.device();
  static const char* kShader = R"(
    @group(0) @binding(0) var<storage, read_write> data : array<u32>;
    @compute @workgroup_size(64) fn main(@builtin(global_invocation_id) id : vec3u) {
      if (id.x < arrayLength(&data)) { data[id.x] = data[id.x] * 2u + 1u; }
    }
  )";
  wgpu::ComputePipelineDescriptor pipelineDescriptor{};
  pipelineDescriptor.compute.module = makeShader(device, kShader);
  pipelineDescriptor.compute.entryPoint = "main";
  wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDescriptor);
  if (!pipeline) {
    error = "CreateComputePipeline returned null";
    return false;
  }

  const uint64_t size = static_cast<uint64_t>(count) * sizeof(uint32_t);
  std::vector<uint32_t> input(count);
  for (uint32_t i = 0; i < count; ++i) input[i] = i * 3u;

  wgpu::BufferDescriptor storageDescriptor{};
  storageDescriptor.size = size;
  storageDescriptor.usage =
      wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer storage = device.CreateBuffer(&storageDescriptor);
  gpu.queue().WriteBuffer(storage, 0, input.data(), size);

  wgpu::BufferDescriptor stagingDescriptor{};
  stagingDescriptor.size = size;
  stagingDescriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer staging = device.CreateBuffer(&stagingDescriptor);

  wgpu::BindGroupEntry entry{};
  entry.binding = 0;
  entry.buffer = storage;
  entry.size = size;
  wgpu::BindGroupDescriptor groupDescriptor{};
  groupDescriptor.layout = pipeline.GetBindGroupLayout(0);
  groupDescriptor.entryCount = 1;
  groupDescriptor.entries = &entry;
  wgpu::BindGroup group = device.CreateBindGroup(&groupDescriptor);

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, group);
  pass.DispatchWorkgroups((count + 63) / 64);
  pass.End();
  encoder.CopyBufferToBuffer(storage, 0, staging, 0, size);
  wgpu::CommandBuffer commands = encoder.Finish();
  gpu.queue().Submit(1, &commands);

  std::vector<uint8_t> bytes;
  if (!mapAndRead(gpu, staging, size, bytes, error)) return false;
  if (!verify) return true;
  const auto* output = reinterpret_cast<const uint32_t*>(bytes.data());
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t expected = input[i] * 2u + 1u;
    if (output[i] != expected) {
      error = "compute mismatch at index " + std::to_string(i) + ": got " +
              std::to_string(output[i]) + ", want " + std::to_string(expected);
      return false;
    }
  }
  return true;
}

}  // namespace rnqnx::webgpu
