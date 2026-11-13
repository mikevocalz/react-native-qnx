#include "GpuContext.h"

#include <mutex>

#include "Log.h"

namespace rnqnx::webgpu {

struct GpuContext::SharedState {
  std::mutex mutex;
  uint64_t generation = 0;
  bool lost = false;
  std::string reason;
  std::atomic<int> uncaptured{0};
};

namespace {

const char* backendName(wgpu::BackendType type) {
  switch (type) {
    case wgpu::BackendType::Null: return "Null";
    case wgpu::BackendType::OpenGLES: return "OpenGLES";
    case wgpu::BackendType::OpenGL: return "OpenGL";
    case wgpu::BackendType::Vulkan: return "Vulkan";
    default: return "Other";
  }
}

const char* adapterTypeName(wgpu::AdapterType type) {
  switch (type) {
    case wgpu::AdapterType::DiscreteGPU: return "DiscreteGPU";
    case wgpu::AdapterType::IntegratedGPU: return "IntegratedGPU";
    case wgpu::AdapterType::CPU: return "CPU";
    default: return "Unknown";
  }
}

const char* lossReasonName(wgpu::DeviceLostReason reason) {
  switch (reason) {
    case wgpu::DeviceLostReason::Unknown: return "Unknown";
    case wgpu::DeviceLostReason::Destroyed: return "Destroyed";
    case wgpu::DeviceLostReason::CallbackCancelled: return "CallbackCancelled";
    case wgpu::DeviceLostReason::FailedCreation: return "FailedCreation";
    default: return "Other";
  }
}

std::string toString(wgpu::StringView view) {
  if (view.data == nullptr) return {};
  if (view.length == WGPU_STRLEN) return std::string(view.data);
  return std::string(view.data, view.length);
}

}  // namespace

std::unique_ptr<GpuContext> GpuContext::create(BackendChoice backend, std::string& error) {
  std::unique_ptr<GpuContext> context(new GpuContext());
  context->state_ = std::make_shared<SharedState>();

  wgpu::InstanceDescriptor instanceDescriptor{};
  // MultipleDevicesPerAdapter lets recreateDevice() ask the same adapter for a
  // second device after the first one is lost.
  static constexpr wgpu::InstanceFeatureName kInstanceFeatures[] = {
      wgpu::InstanceFeatureName::TimedWaitAny, wgpu::InstanceFeatureName::MultipleDevicesPerAdapter};
  instanceDescriptor.requiredFeatureCount = 2;
  instanceDescriptor.requiredFeatures = kInstanceFeatures;
  context->instance_ = wgpu::CreateInstance(&instanceDescriptor);
  if (!context->instance_) {
    error = "wgpu::CreateInstance returned null";
    return nullptr;
  }

  wgpu::RequestAdapterOptions options{};
  options.backendType =
      backend == BackendChoice::OpenGLES ? wgpu::BackendType::OpenGLES : wgpu::BackendType::Null;
  // Dawn's OpenGL ES backend only offers the WebGPU compatibility feature level.
  options.featureLevel = backend == BackendChoice::OpenGLES ? wgpu::FeatureLevel::Compatibility
                                                            : wgpu::FeatureLevel::Core;

  // The callback owns its result: if wait() times out this function returns,
  // and Dawn may still complete the request later.
  struct AdapterResult {
    bool done = false;
    wgpu::Adapter adapter;
    std::string error;
  };
  auto adapterResult = std::make_shared<AdapterResult>();
  auto future = context->instance_.RequestAdapter(
      &options, wgpu::CallbackMode::WaitAnyOnly,
      [adapterResult](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
                      wgpu::StringView message) {
        adapterResult->done = true;
        if (status == wgpu::RequestAdapterStatus::Success) {
          adapterResult->adapter = std::move(adapter);
        } else {
          adapterResult->error = toString(message);
        }
      });
  if (!context->wait(future) || !adapterResult->done) {
    error = "RequestAdapter timed out";
    return nullptr;
  }
  if (!adapterResult->adapter) {
    error = "RequestAdapter failed: " + adapterResult->error;
    return nullptr;
  }
  context->adapter_ = std::move(adapterResult->adapter);
  if (!context->requestDevice(error)) return nullptr;
  return context;
}

bool GpuContext::requestDevice(std::string& error) {
  uint64_t generation;
  {
    std::lock_guard lock(state_->mutex);
    generation = ++state_->generation;
    state_->lost = false;
    state_->reason.clear();
  }
  deviceLost_.store(false);

  wgpu::DeviceDescriptor descriptor{};
  descriptor.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [state = state_, generation](const wgpu::Device&, wgpu::DeviceLostReason reason,
                                   wgpu::StringView message) {
        std::lock_guard lock(state->mutex);
        const std::string text = std::string(lossReasonName(reason)) + ": " + toString(message);
        logLine("DEVICE", "lost generation=%llu reason=%s", (unsigned long long)generation,
                text.c_str());
        // A loss on an older device (destroyed when we replaced it) is not news.
        if (generation == state->generation) {
          state->lost = true;
          state->reason = text;
        }
      });
  // Repeatable callbacks cannot capture, so the state travels as userdata. state_
  // is the first GpuContext member and therefore outlives the device.
  descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message,
         SharedState* state) {
        state->uncaptured.fetch_add(1);
        logLine("DEVICE", "uncaptured_error type=%d message=\"%s\"", static_cast<int>(type),
                toString(message).c_str());
      },
      state_.get());

  struct DeviceResult {
    bool done = false;
    wgpu::Device device;
    std::string error;
  };
  auto result = std::make_shared<DeviceResult>();
  auto future = adapter_.RequestDevice(
      &descriptor, wgpu::CallbackMode::WaitAnyOnly,
      [result](wgpu::RequestDeviceStatus status, wgpu::Device created, wgpu::StringView message) {
        result->done = true;
        if (status == wgpu::RequestDeviceStatus::Success) {
          result->device = std::move(created);
        } else {
          result->error = toString(message);
        }
      });
  if (!wait(future) || !result->done) {
    error = "RequestDevice timed out";
    return false;
  }
  if (!result->device) {
    error = "RequestDevice failed: " + result->error;
    return false;
  }
  device_ = std::move(result->device);
  queue_ = device_.GetQueue();
  return true;
}

bool GpuContext::wait(wgpu::Future future, uint64_t timeoutNs) const {
  wgpu::FutureWaitInfo info{future, false};
  const auto status = instance_.WaitAny(1, &info, timeoutNs);
  return status == wgpu::WaitStatus::Success && info.completed;
}

void GpuContext::logCapabilities() const {
  wgpu::AdapterInfo info{};
  adapter_.GetInfo(&info);
  logLine("ADAPTER", "backend=%s type=%s vendor=\"%s\" architecture=\"%s\" device=\"%s\" "
                     "description=\"%s\" vendorId=0x%x deviceId=0x%x",
          backendName(info.backendType), adapterTypeName(info.adapterType),
          toString(info.vendor).c_str(), toString(info.architecture).c_str(),
          toString(info.device).c_str(), toString(info.description).c_str(), info.vendorID,
          info.deviceID);

  wgpu::Limits adapterLimits{};
  adapter_.GetLimits(&adapterLimits);
  wgpu::Limits l{};
  device_.GetLimits(&l);
  logLine("LIMITS",
          "adapter.maxTextureDimension2D=%u device.maxTextureDimension1D=%u "
          "maxTextureDimension2D=%u maxTextureDimension3D=%u maxTextureArrayLayers=%u "
          "maxBindGroups=%u maxBindingsPerBindGroup=%u",
          adapterLimits.maxTextureDimension2D, l.maxTextureDimension1D, l.maxTextureDimension2D,
          l.maxTextureDimension3D, l.maxTextureArrayLayers, l.maxBindGroups,
          l.maxBindingsPerBindGroup);
  logLine("LIMITS",
          "maxSampledTexturesPerShaderStage=%u maxSamplersPerShaderStage=%u "
          "maxStorageBuffersPerShaderStage=%u maxStorageTexturesPerShaderStage=%u "
          "maxUniformBuffersPerShaderStage=%u maxUniformBufferBindingSize=%llu "
          "maxStorageBufferBindingSize=%llu",
          l.maxSampledTexturesPerShaderStage, l.maxSamplersPerShaderStage,
          l.maxStorageBuffersPerShaderStage, l.maxStorageTexturesPerShaderStage,
          l.maxUniformBuffersPerShaderStage, (unsigned long long)l.maxUniformBufferBindingSize,
          (unsigned long long)l.maxStorageBufferBindingSize);
  logLine("LIMITS",
          "minUniformBufferOffsetAlignment=%u minStorageBufferOffsetAlignment=%u "
          "maxVertexBuffers=%u maxBufferSize=%llu maxVertexAttributes=%u "
          "maxVertexBufferArrayStride=%u maxInterStageShaderVariables=%u maxColorAttachments=%u "
          "maxColorAttachmentBytesPerSample=%u",
          l.minUniformBufferOffsetAlignment, l.minStorageBufferOffsetAlignment, l.maxVertexBuffers,
          (unsigned long long)l.maxBufferSize, l.maxVertexAttributes, l.maxVertexBufferArrayStride,
          l.maxInterStageShaderVariables, l.maxColorAttachments, l.maxColorAttachmentBytesPerSample);
  logLine("LIMITS",
          "maxComputeWorkgroupStorageSize=%u maxComputeInvocationsPerWorkgroup=%u "
          "maxComputeWorkgroupSizeX=%u maxComputeWorkgroupSizeY=%u maxComputeWorkgroupSizeZ=%u "
          "maxComputeWorkgroupsPerDimension=%u",
          l.maxComputeWorkgroupStorageSize, l.maxComputeInvocationsPerWorkgroup,
          l.maxComputeWorkgroupSizeX, l.maxComputeWorkgroupSizeY, l.maxComputeWorkgroupSizeZ,
          l.maxComputeWorkgroupsPerDimension);

  wgpu::SupportedFeatures features;
  device_.GetFeatures(&features);
  std::string list;
  for (size_t i = 0; i < features.featureCount; ++i) {
    if (!list.empty()) list += ",";
    list += std::to_string(static_cast<uint32_t>(features.features[i]));
  }
  logLine("FEATURES", "count=%zu enum_values=[%s]", features.featureCount, list.c_str());
}

std::string GpuContext::lossReason() const {
  std::lock_guard lock(state_->mutex);
  return state_->reason;
}

bool GpuContext::injectDeviceLoss() {
  const wgpu::Future lostFuture = device_.GetLostFuture();
  device_.ForceLoss(wgpu::DeviceLostReason::Unknown, "rnqnx injected loss");
  if (!wait(lostFuture)) return false;
  std::lock_guard lock(state_->mutex);
  // Only the loss this call injected counts: reason Unknown with our message.
  const bool injected = state_->lost && state_->reason.find("Unknown") == 0 &&
                        state_->reason.find("rnqnx injected loss") != std::string::npos;
  deviceLost_.store(injected);
  return injected;
}

bool GpuContext::recreateDevice(std::string& error) {
  queue_ = nullptr;
  device_ = nullptr;
  return requestDevice(error);
}

int GpuContext::takeUncapturedErrors() { return state_->uncaptured.exchange(0); }

}  // namespace rnqnx::webgpu
