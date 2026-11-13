#pragma once

#include <jsi/jsi.h>

#include <vector>

namespace rnqnx::expo::image {

facebook::jsi::Function hostFunction(
    facebook::jsi::Runtime& runtime,
    const char* name,
    unsigned int arguments,
    facebook::jsi::HostFunctionType body);

/// `Promise.resolve(value)`.
facebook::jsi::Value resolvedPromise(facebook::jsi::Runtime& runtime, facebook::jsi::Value value);

/// `promise.then(onFulfilled)`; a rejection passes through.
facebook::jsi::Value thenPromise(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::Value& promise,
    facebook::jsi::HostFunctionType onFulfilled);

/// Calls a method of the renderer's `ImageLoader` TurboModule. Throws a JS error
/// when the host has no such module.
facebook::jsi::Value callImageLoader(
    facebook::jsi::Runtime& runtime,
    const char* method,
    std::vector<facebook::jsi::Value> args);

/// `Image.loadAsync`, `generateBlurhashAsync`, `generateThumbhashAsync`,
/// `writeToCacheAsync`, `readFromCacheAsync` and the `ImageRef` class they
/// return (ExpoImageRefs.cpp).
void installImageRefs(facebook::jsi::Runtime& runtime, facebook::jsi::Object& module);

/// The functions expo-image calls on a view ref (ExpoImageViewCommands.cpp).
void installViewPrototypes(facebook::jsi::Runtime& runtime, facebook::jsi::Object& module);

}  // namespace rnqnx::expo::image
