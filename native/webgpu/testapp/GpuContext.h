// Owns the Dawn instance, adapter, device and queue, and reports what the
// adapter exposes. Device loss is observable and recoverable: recreateDevice()
// builds a new device from the same adapter.
#pragma once

#include <dawn/webgpu_cpp.h>

#include <atomic>
#include <memory>
#include <string>

namespace rnqnx::webgpu {

enum class BackendChoice { OpenGLES, Null };

class GpuContext {
 public:
  // Returns null and fills `error` when no adapter or device could be made.
  static std::unique_ptr<GpuContext> create(BackendChoice backend, std::string& error);

  const wgpu::Instance& instance() const { return instance_; }
  const wgpu::Adapter& adapter() const { return adapter_; }
  const wgpu::Device& device() const { return device_; }
  const wgpu::Queue& queue() const { return queue_; }

  // Logs adapter info, the limits the device actually got, and its features.
  void logCapabilities() const;

  // Blocks until `future` completes. Returns false on timeout.
  bool wait(wgpu::Future future, uint64_t timeoutNs = 5'000'000'000ull) const;

  bool deviceLost() const { return deviceLost_.load(); }
  std::string lossReason() const;
  // Injects a loss with Device::ForceLoss and waits for the callback.
  bool injectDeviceLoss();
  // Requests a fresh device from the same adapter. The old device is dropped.
  bool recreateDevice(std::string& error);

  // Counts uncaptured errors since the last call (and resets the count).
  int takeUncapturedErrors();

 private:
  struct SharedState;

  GpuContext() = default;
  bool requestDevice(std::string& error);

  // Declared first so it is destroyed last: Dawn callbacks may still fire while
  // the device and adapter are being released.
  std::shared_ptr<SharedState> state_;
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::Queue queue_;
  std::atomic<bool> deviceLost_{false};
};

}  // namespace rnqnx::webgpu
