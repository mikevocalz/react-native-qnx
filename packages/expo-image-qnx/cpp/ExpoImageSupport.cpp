#include "ExpoImageSupport.hpp"

#include <utility>

namespace rnqnx::expo::image {

namespace jsi = facebook::jsi;

jsi::Function hostFunction(jsi::Runtime& runtime, const char* name, unsigned int arguments,
                           jsi::HostFunctionType body) {
  return jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, name), arguments, std::move(body));
}

jsi::Value resolvedPromise(jsi::Runtime& runtime, jsi::Value value) {
  auto promise = runtime.global().getPropertyAsFunction(runtime, "Promise");
  return promise.getPropertyAsFunction(runtime, "resolve")
      .callWithThis(runtime, promise, std::move(value));
}

jsi::Value thenPromise(jsi::Runtime& runtime, const jsi::Value& promise, jsi::HostFunctionType onFulfilled) {
  auto object = promise.asObject(runtime);
  auto then = object.getPropertyAsFunction(runtime, "then");
  return then.callWithThis(runtime, object, hostFunction(runtime, "onFulfilled", 1, std::move(onFulfilled)));
}

namespace {

// The renderer's ImageLoader TurboModule, or an error when this host has none.
// A bridgeless host (this renderer) exposes modules as properties of the
// `nativeModuleProxy` host object; a bridge host has `__turboModuleProxy(name)`.
jsi::Object imageLoader(jsi::Runtime& runtime) {
  jsi::Value loader = jsi::Value::undefined();
  auto proxy = runtime.global().getProperty(runtime, "nativeModuleProxy");
  if (proxy.isObject()) {
    loader = proxy.asObject(runtime).getProperty(runtime, "ImageLoader");
  } else {
    auto function = runtime.global().getProperty(runtime, "__turboModuleProxy");
    if (function.isObject() && function.asObject(runtime).isFunction(runtime)) {
      loader = function.asObject(runtime).asFunction(runtime).call(
          runtime, jsi::String::createFromAscii(runtime, "ImageLoader"));
    }
  }
  if (!loader.isObject()) {
    throw jsi::JSError(runtime, "expo-image needs the ImageLoader module, which this host lacks");
  }
  return loader.asObject(runtime);
}

}  // namespace

jsi::Value callImageLoader(jsi::Runtime& runtime, const char* method, std::vector<jsi::Value> args) {
  auto loader = imageLoader(runtime);
  auto function = loader.getPropertyAsFunction(runtime, method);
  return function.callWithThis(runtime, loader, static_cast<const jsi::Value*>(args.data()), args.size());
}

}  // namespace rnqnx::expo::image
