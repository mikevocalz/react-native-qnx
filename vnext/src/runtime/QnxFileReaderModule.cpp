#include "react-native-qnx/runtime/QnxFileReaderModule.h"

namespace rnqnx::runtime {

QnxFileReaderModule::QnxFileReaderModule(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)) {
  registerMethods();
}

void QnxFileReaderModule::registerMethods() {
  methodMap_["readAsDataURL"] = {1, &QnxFileReaderModule::readAsDataURL};
  methodMap_["readAsText"] = {2, &QnxFileReaderModule::readAsText};
}

namespace {

facebook::jsi::Value rejectNotImplemented(facebook::jsi::Runtime& rt) {
  auto Promise = rt.global().getPropertyAsObject(rt, "Promise").asFunction(rt);
  auto executor = facebook::jsi::Function::createFromHostFunction(
      rt,
      facebook::jsi::PropNameID::forAscii(rt, "executor"),
      2,
      [](facebook::jsi::Runtime& r,
         const facebook::jsi::Value&,
         const facebook::jsi::Value* args,
         size_t) {
        auto reject = args[1].asObject(r).asFunction(r);
        auto Error = r.global().getPropertyAsObject(r, "Error");
        auto error = Error.asFunction(r).callAsConstructor(
            r,
            facebook::jsi::String::createFromUtf8(
                r, "FileReaderModule is not implemented on this platform"));
        facebook::jsi::Value errorValue(r, error);
        reject.call(
            r,
            static_cast<const facebook::jsi::Value*>(&errorValue),
            static_cast<size_t>(1));
        return facebook::jsi::Value::undefined();
      });
  return Promise.callAsConstructor(rt, executor);
}

} // namespace

facebook::jsi::Value QnxFileReaderModule::readAsDataURL(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return rejectNotImplemented(rt);
}

facebook::jsi::Value QnxFileReaderModule::readAsText(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& /*tm*/,
    const facebook::jsi::Value* /*args*/,
    size_t /*count*/) {
  return rejectNotImplemented(rt);
}

} // namespace rnqnx::runtime
