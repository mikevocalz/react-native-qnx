#include "react-native-qnx/runtime/QnxSegmentFetcher.h"

namespace rnqnx::runtime {

QnxSegmentFetcher::QnxSegmentFetcher(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)) {
  registerMethods();
}

void QnxSegmentFetcher::registerMethods() {
  methodMap_["fetchSegment"] = {3, &QnxSegmentFetcher::fetchSegment};
  methodMap_["getSegment"] = {3, &QnxSegmentFetcher::getSegment};
}

namespace {

void invokeErrorCallback(facebook::jsi::Runtime& rt, const facebook::jsi::Value* args) {
  // The third argument is the error callback.
  if (!args[2].isObject()) {
    return;
  }
  auto callback = args[2].asObject(rt).asFunction(rt);
  auto Error = rt.global().getPropertyAsObject(rt, "Error");
  auto error = Error.asFunction(rt).callAsConstructor(
      rt,
      facebook::jsi::String::createFromUtf8(
          rt, "SegmentFetcher is not implemented on this platform"));
  facebook::jsi::Value errorValue(rt, error);
  callback.call(
      rt,
      static_cast<const facebook::jsi::Value*>(&errorValue),
      static_cast<size_t>(1));
}

} // namespace

facebook::jsi::Value QnxSegmentFetcher::fetchSegment(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* args,
    size_t /*count*/) {
  invokeErrorCallback(rt, args);
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxSegmentFetcher::getSegment(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* args,
    size_t /*count*/) {
  invokeErrorCallback(rt, args);
  return facebook::jsi::Value::undefined();
}

} // namespace rnqnx::runtime
