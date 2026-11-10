#pragma once
#include <ReactCommon/TurboModule.h>
#include <memory>
#include <functional>
class QQuickWindow;
namespace rnqnx::runtime {
// Exact Worklets 0.13.1 bootstrap contract. Explicit experimental exception to
// ADR-0004 for this third-party library, not a new application TurboModule API.
class QnxWorkletsModule final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "WorkletsModule";
  QnxWorkletsModule(facebook::jsi::Runtime& runtime,
      std::shared_ptr<facebook::react::CallInvoker> invoker,
      std::function<void(std::function<void()>)> postToOwner);
  ~QnxWorkletsModule() override;
  // Host-stop-only: call on owner after surface/app teardown, before RN
  // destruction. Not a public shutdown/reopen or reusable module API.
  void invalidate();
  void setWindow(QQuickWindow* window);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  static facebook::jsi::Value install(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value start(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
  static facebook::jsi::Value unsupported(facebook::jsi::Runtime&,
      facebook::react::TurboModule&, const facebook::jsi::Value*, size_t);
};
}
