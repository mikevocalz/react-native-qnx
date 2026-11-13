// The functions expo-image calls on a view ref: `ref.startAnimating()` and the
// rest reach `ViewPrototypes.ExpoImage` of this module, because
// expo-modules-core assigns them to the view component's prototype. Each one
// dispatches a Fabric view command to the mounted view through
// `expo.dispatchViewCommand` (the QNX polyfill), which calls React Native's
// `dispatchCommand`. The renderer's mounting layer runs the command on the
// ExpoImageItem (see ExpoImageNativeComponent.ts for the command list).
#include "ExpoImageSupport.hpp"

namespace rnqnx::expo::image {
namespace {

namespace jsi = facebook::jsi;

void dispatch(jsi::Runtime& runtime, const jsi::Value& view, const char* command) {
  auto expo = runtime.global().getProperty(runtime, "expo");
  jsi::Value dispatcher = expo.isObject() ? expo.asObject(runtime).getProperty(runtime, "dispatchViewCommand")
                                          : jsi::Value::undefined();
  if (!dispatcher.isObject() || !dispatcher.asObject(runtime).isFunction(runtime)) {
    throw jsi::JSError(runtime, "expo.dispatchViewCommand is missing: the QNX Expo polyfill did not load");
  }
  if (!view.isObject()) throw jsi::JSError(runtime, "The view function needs a view ref");
  dispatcher.asObject(runtime).asFunction(runtime).callWithThis(
      runtime, expo.asObject(runtime), view, jsi::String::createFromAscii(runtime, command),
      jsi::Array(runtime, 0));
}

}  // namespace

void installViewPrototypes(jsi::Runtime& runtime, jsi::Object& module) {
  jsi::Object prototypes(runtime);
  jsi::Object imageView(runtime);
  // Playback control returns at once; the *Async functions return a Promise.
  struct Entry {
    const char* name;
    const char* command;
    bool async;
  };
  for (const Entry entry : {Entry{"startAnimating", "startAnimating", false},
                            Entry{"stopAnimating", "stopAnimating", false},
                            Entry{"lockResourceAsync", "lockResource", true},
                            Entry{"unlockResourceAsync", "unlockResource", true},
                            Entry{"reloadAsync", "reload", true}}) {
    imageView.setProperty(
        runtime, entry.name,
        hostFunction(runtime, entry.name, 0,
                     [entry](jsi::Runtime& rt, const jsi::Value& thisValue, const jsi::Value*, size_t) {
                       dispatch(rt, thisValue, entry.command);
                       return entry.async ? resolvedPromise(rt, jsi::Value::undefined())
                                          : jsi::Value::undefined();
                     }));
  }
  prototypes.setProperty(runtime, "ExpoImage", std::move(imageView));
  module.setProperty(runtime, "ViewPrototypes", std::move(prototypes));
}

}  // namespace rnqnx::expo::image
