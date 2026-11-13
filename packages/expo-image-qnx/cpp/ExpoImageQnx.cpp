// ExpoImage for QNX: the native module expo-image 58 requires at import time,
// and the view config its requireNativeView('ExpoImage') reads.
//
// The view is drawn by the renderer's Fabric component
// ViewManagerAdapter_ExpoImage (RNQNX_ENABLE_QT_EXPO_IMAGE), on the same Qt
// image loader and decoded cache as React Native's own Image. The module's
// cache functions do not reach that cache from here; see README.md.
#include "ExpoQnxModuleRegistry.hpp"

#include <jsi/jsi.h>

#include <string>
#include <utility>

namespace rnqnx::expo::image {
namespace {

namespace jsi = facebook::jsi;

// The props expo-image's ExpoImage.tsx passes to the native view: its own
// (ImageNativeProps) and the style and color values it spreads onto the view.
ExpoViewConfig viewConfig() {
  ExpoViewConfig config;
  config.props = {
      "source",
      "placeholder",
      "contentFit",
      "contentPosition",
      "transition",
      "tintColor",
      "blurRadius",
      "priority",
      "cachePolicy",
      "recyclingKey",
      "allowDownscaling",
      "autoplay",
      "enableLiveTextInteraction",
      "accessibilityLabel",
      "accessible",
      "borderColor",
      "borderTopColor",
      "borderRightColor",
      "borderBottomColor",
      "borderLeftColor",
      "borderStartColor",
      "borderEndColor",
      "backgroundColor",
  };
  config.events = {"onLoadStart", "onProgress", "onLoad", "onError", "onDisplay"};
  return config;
}

jsi::Value resolved(jsi::Runtime& runtime, jsi::Value value) {
  auto promise = runtime.global().getPropertyAsFunction(runtime, "Promise");
  return promise.getPropertyAsFunction(runtime, "resolve")
      .callWithThis(runtime, promise, std::move(value));
}

jsi::Function function(jsi::Runtime& runtime, const char* name, unsigned int arguments,
                       jsi::HostFunctionType body) {
  return jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, name), arguments, std::move(body));
}

jsi::Object createModule(jsi::Runtime& runtime) {
  jsi::Object module(runtime);
  module.setProperty(runtime, "name", "ExpoImage");

  // Cache controls. The image cache is the renderer's decoded cache, which
  // this package cannot reach, so each one reports "nothing done": false for
  // the boolean results, null for the path. None of them invents success.
  for (const char* name : {"clearMemoryCache", "clearDiskCache", "prefetch"}) {
    module.setProperty(runtime, name,
        function(runtime, name, 0, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
          return resolved(rt, jsi::Value(false));
        }));
  }
  module.setProperty(runtime, "getCachePathAsync",
      function(runtime, "getCachePathAsync", 1, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
        return resolved(rt, jsi::Value::null());
      }));
  module.setProperty(runtime, "configureCache",
      function(runtime, "configureCache", 1, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
        return jsi::Value::undefined();
      }));
  // Blurhash, thumbhash, loadAsync and the cache read/write functions need
  // native decoders and are absent: calling one fails with "is not a function"
  // instead of returning invented data.

  // Functions expo-image calls on the view ref. The view shows no animated
  // images and holds no resource lock, so these have nothing to do.
  jsi::Object prototypes(runtime);
  jsi::Object imageView(runtime);
  for (const char* name : {"startAnimating", "stopAnimating"}) {
    imageView.setProperty(runtime, name,
        function(runtime, name, 0, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
          return jsi::Value::undefined();
        }));
  }
  for (const char* name : {"lockResourceAsync", "unlockResourceAsync", "reloadAsync"}) {
    imageView.setProperty(runtime, name,
        function(runtime, name, 0, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
          return resolved(rt, jsi::Value::undefined());
        }));
  }
  prototypes.setProperty(runtime, "ExpoImage", std::move(imageView));
  module.setProperty(runtime, "ViewPrototypes", std::move(prototypes));
  return module;
}

} // namespace

void registerModule() {
  auto& registry = ExpoQnxModuleRegistry::instance();
  registry.registerModule("ExpoImage", createModule);
  registry.registerViewConfig("ExpoImage", "", viewConfig());
}

} // namespace rnqnx::expo::image
