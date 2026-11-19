// expo-image's ImageRef functions on QNX. The pictures live in the renderer
// (ImageRefRegistry), so each function forwards to the ImageLoader TurboModule
// and wraps what comes back: an ImageRef is a SharedRef JavaScript object whose
// hidden `__expo_shared_object_id__` is the registry id. expo-modules-core
// sends that id to the ExpoImage view when an ImageRef is its `source`.
#include "ExpoImageSupport.hpp"

#include <string>
#include <utility>
#include <vector>

namespace rnqnx::expo::image {
namespace {

namespace jsi = facebook::jsi;

constexpr const char* kIdProperty = "__expo_shared_object_id__";

// The prototype of every ImageRef (`ImageModule.Image.prototype`).
jsi::Object imageRefPrototype(jsi::Runtime& runtime, const jsi::Object& module) {
  return module.getProperty(runtime, "Image").asObject(runtime).getProperty(runtime, "prototype").asObject(runtime);
}

// Links the prototype under SharedRef the first time it is needed. SharedRef is
// defined by the polyfill, which runs after this module is created.
void adoptSharedRef(jsi::Runtime& runtime, jsi::Object& prototype) {
  auto expo = runtime.global().getProperty(runtime, "expo");
  auto sharedRef = expo.isObject() ? expo.asObject(runtime).getProperty(runtime, "SharedRef") : jsi::Value::undefined();
  if (!sharedRef.isObject()) throw jsi::JSError(runtime, "expo.SharedRef is missing: the QNX Expo polyfill did not load");
  auto base = sharedRef.asObject(runtime).getProperty(runtime, "prototype");
  auto objectClass = runtime.global().getPropertyAsObject(runtime, "Object");
  objectClass.getPropertyAsFunction(runtime, "setPrototypeOf")
      .callWithThis(runtime, objectClass, prototype, std::move(base));
}

// An ImageRef for the descriptor ImageLoader.loadImage / readFromCache returns:
// { id, width, height, scale, mediaType, isAnimated }.
jsi::Value makeImageRef(jsi::Runtime& runtime, const jsi::Object& module, const jsi::Value& descriptor) {
  if (!descriptor.isObject()) return jsi::Value::null();
  const auto fields = descriptor.getObject(runtime);
  jsi::Object prototype = imageRefPrototype(runtime, module);
  adoptSharedRef(runtime, prototype);
  auto objectClass = runtime.global().getPropertyAsObject(runtime, "Object");
  auto ref = objectClass.getPropertyAsFunction(runtime, "create")
                 .callWithThis(runtime, objectClass, prototype)
                 .asObject(runtime);
  const auto id = fields.getProperty(runtime, "id");
  ref.setProperty(runtime, kIdProperty, jsi::Value(runtime, id));
  for (const char* name : {"width", "height", "scale", "mediaType", "isAnimated"}) {
    ref.setProperty(runtime, name, fields.getProperty(runtime, name));
  }
  ref.setProperty(runtime, "nativeRefType", "image");
  return jsi::Value(std::move(ref));
}

// What the loader takes as a source: an ImageRef's id, else the value itself.
jsi::Value loaderSource(jsi::Runtime& runtime, const jsi::Value& source) {
  if (source.isObject()) {
    auto id = source.getObject(runtime).getProperty(runtime, kIdProperty);
    if (id.isNumber()) return id;
  }
  return jsi::Value(runtime, source);
}

jsi::Value argument(jsi::Runtime& runtime, const jsi::Value* args, size_t count, size_t index) {
  return index < count ? jsi::Value(runtime, args[index]) : jsi::Value::undefined();
}

// numberOfComponents: [x, y] or { width, height }; expo-image's default is [4, 3].
bool readComponents(jsi::Runtime& runtime, const jsi::Value& value, double& x, double& y) {
  x = 4;
  y = 3;
  if (!value.isObject()) return value.isUndefined() || value.isNull();
  const auto object = value.getObject(runtime);
  jsi::Value first, second;
  if (object.isArray(runtime)) {
    const auto array = object.getArray(runtime);
    if (array.size(runtime) != 2) return false;
    first = array.getValueAtIndex(runtime, 0);
    second = array.getValueAtIndex(runtime, 1);
  } else {
    first = object.getProperty(runtime, "width");
    second = object.getProperty(runtime, "height");
  }
  if (!first.isNumber() || !second.isNumber()) return false;
  x = first.getNumber();
  y = second.getNumber();
  return true;
}

}  // namespace

void installImageRefs(jsi::Runtime& runtime, jsi::Object& module) {
  // `Image.Image` is the ImageRef class. A ref comes from the functions below,
  // never from `new`.
  auto refClass = hostFunction(runtime, "ImageRef", 0, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) -> jsi::Value {
    throw jsi::JSError(rt, "An ImageRef is created by Image.loadAsync, useImage or readFromCacheAsync");
  });
  refClass.setProperty(runtime, "prototype", jsi::Object(runtime));
  // release() frees the picture the ref holds.
  auto prototype = refClass.getProperty(runtime, "prototype").asObject(runtime);
  prototype.setProperty(runtime, "release",
      hostFunction(runtime, "release", 0, [](jsi::Runtime& rt, const jsi::Value& self, const jsi::Value*, size_t) {
        if (!self.isObject()) return jsi::Value::undefined();
        auto id = self.getObject(rt).getProperty(rt, kIdProperty);
        if (id.isNumber()) {
          std::vector<jsi::Value> forwarded;
          forwarded.push_back(std::move(id));
          callImageLoader(rt, "releaseImage", std::move(forwarded));
        }
        return jsi::Value::undefined();
      }));
  module.setProperty(runtime, "Image", std::move(refClass));

  // The module object outlives every call: it is the one the runtime holds.
  // Functions look it up through `expo.modules.ExpoImage` so they hold no JSI
  // value of their own.
  const auto moduleObject = [](jsi::Runtime& rt) {
    return rt.global().getPropertyAsObject(rt, "expo").getPropertyAsObject(rt, "modules").getPropertyAsObject(rt, "ExpoImage");
  };

  module.setProperty(runtime, "loadAsync",
      hostFunction(runtime, "loadAsync", 2, [moduleObject](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        std::vector<jsi::Value> forwarded;
        forwarded.push_back(loaderSource(rt, argument(rt, args, count, 0)));
        forwarded.push_back(argument(rt, args, count, 1));
        auto pending = callImageLoader(rt, "loadImage", std::move(forwarded));
        return thenPromise(rt, pending, [moduleObject](jsi::Runtime& inner, const jsi::Value&, const jsi::Value* result, size_t n) {
          return makeImageRef(inner, moduleObject(inner), n > 0 ? jsi::Value(inner, result[0]) : jsi::Value::null());
        });
      }));

  module.setProperty(runtime, "readFromCacheAsync",
      hostFunction(runtime, "readFromCacheAsync", 1, [moduleObject](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        std::vector<jsi::Value> forwarded;
        forwarded.push_back(argument(rt, args, count, 0));
        auto pending = callImageLoader(rt, "readFromCache", std::move(forwarded));
        return thenPromise(rt, pending, [moduleObject](jsi::Runtime& inner, const jsi::Value&, const jsi::Value* result, size_t n) {
          return makeImageRef(inner, moduleObject(inner), n > 0 ? jsi::Value(inner, result[0]) : jsi::Value::null());
        });
      }));

  module.setProperty(runtime, "writeToCacheAsync",
      hostFunction(runtime, "writeToCacheAsync", 2, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        std::vector<jsi::Value> forwarded;
        forwarded.push_back(loaderSource(rt, argument(rt, args, count, 0)));
        forwarded.push_back(argument(rt, args, count, 1));
        auto pending = callImageLoader(rt, "writeToCache", std::move(forwarded));
        return thenPromise(rt, pending, [](jsi::Runtime&, const jsi::Value&, const jsi::Value*, size_t) {
          return jsi::Value::undefined();
        });
      }));

  module.setProperty(runtime, "generateBlurhashAsync",
      hostFunction(runtime, "generateBlurhashAsync", 2, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        double x = 4, y = 3;
        if (!readComponents(rt, argument(rt, args, count, 1), x, y)) {
          return resolvedPromise(rt, jsi::Value::null());
        }
        std::vector<jsi::Value> forwarded;
        forwarded.push_back(loaderSource(rt, argument(rt, args, count, 0)));
        forwarded.emplace_back(x);
        forwarded.emplace_back(y);
        return callImageLoader(rt, "generateBlurhash", std::move(forwarded));
      }));

  module.setProperty(runtime, "generateThumbhashAsync",
      hostFunction(runtime, "generateThumbhashAsync", 1, [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
        std::vector<jsi::Value> forwarded;
        forwarded.push_back(loaderSource(rt, argument(rt, args, count, 0)));
        return callImageLoader(rt, "generateThumbhash", std::move(forwarded));
      }));
}

}  // namespace rnqnx::expo::image
