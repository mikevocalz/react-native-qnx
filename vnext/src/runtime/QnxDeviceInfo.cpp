#include "react-native-qnx/runtime/QnxDeviceInfo.h"

#include <QThread>

#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace {
jsi::Object displayMetrics(jsi::Runtime& runtime, const QSizeF& size,
                          double scale, double fontScale) {
  auto result = jsi::Object(runtime);
  result.setProperty(runtime, "width", size.width());
  result.setProperty(runtime, "height", size.height());
  result.setProperty(runtime, "scale", scale);
  result.setProperty(runtime, "fontScale", fontScale);
  return result;
}

jsi::Object dimensions(jsi::Runtime& runtime, const WindowMetrics& metrics) {
  auto result = jsi::Object(runtime);
  // These are RN's logical metric fields. Supplying Qt logical geometry as
  // PhysicalPixels would incorrectly divide it by DPR again in Dimensions.js.
  result.setProperty(runtime, "window",
      displayMetrics(runtime, metrics.windowSize, metrics.windowScale, metrics.fontScale));
  result.setProperty(runtime, "screen",
      displayMetrics(runtime, metrics.screenSize, metrics.screenScale, metrics.fontScale));
  return result;
}
}  // namespace

struct QnxDeviceInfo::State {
  explicit State(const WindowMetrics& initial)
      : owner(QThread::currentThread()), current(initial) {}
  void requireOwner() const noexcept {
    if (QThread::currentThread() != owner) std::terminate();
  }
  QThread* const owner;
  WindowMetrics current;
  bool active = true;
};

QnxDeviceInfo::QnxDeviceInfo(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker,
    const WindowMetrics& initial)
    : NativeDeviceInfoCxxSpec(jsInvoker), state_(std::make_shared<State>(initial)),
      invoker_(std::move(jsInvoker)) {
  if (!invoker_) throw std::invalid_argument("DeviceInfo requires the runtime invoker");
}

QnxDeviceInfo::~QnxDeviceInfo() {
  // QtReactHost invalidates on the owner before closing its scheduler. A module
  // can then be released later without touching the Qt owner or a JS runtime.
  if (state_->active) invalidate();
}

jsi::Object QnxDeviceInfo::getConstants(jsi::Runtime& runtime) {
  state_->requireOwner();
  if (!state_->active) throw jsi::JSError(runtime, "DeviceInfo observation is closed");
  auto constants = jsi::Object(runtime);
  constants.setProperty(runtime, "Dimensions", dimensions(runtime, state_->current));
  return constants;
}

void QnxDeviceInfo::updateMetrics(const WindowMetrics& metrics) {
  state_->requireOwner();
  if (!state_->active || metrics == state_->current) return;
  state_->current = metrics;
  const std::weak_ptr<State> weak = state_;
  invoker_->invokeAsync([weak, metrics](jsi::Runtime& runtime) {
    const auto live = weak.lock();
    if (!live || !live->active) return;
    live->requireOwner();
    // A later native update or initial constants read may precede delivery of
    // queued work. Never publish an older snapshot after the latest one.
    if (metrics != live->current) return;
    const auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
    if (!emitter.isObject()) return; // Constants still contain the latest snapshot.
    auto object = emitter.asObject(runtime);
    object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
        jsi::String::createFromAscii(runtime, "didUpdateDimensions"), dimensions(runtime, metrics));
  });
}

void QnxDeviceInfo::invalidate() noexcept {
  if (!state_->active) return;
  state_->requireOwner();
  state_->active = false;
}

}  // namespace rnqnx::runtime
