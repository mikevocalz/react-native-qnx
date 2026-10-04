#include "react-native-qnx/runtime/QnxImageLoader.h"

#include <react/bridging/Dynamic.h>
#include <react/bridging/Promise.h>

namespace rnqnx::runtime {

QnxImageLoader::QnxImageLoader(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)) {
  registerMethods();
}

void QnxImageLoader::registerMethods() {
  methodMap_["getConstants"] = {0, &QnxImageLoader::getConstants};
  methodMap_["getSize"] = {1, &QnxImageLoader::getSize};
  methodMap_["getSizeWithHeaders"] = {2, &QnxImageLoader::getSizeWithHeaders};
  methodMap_["prefetchImage"] = {1, &QnxImageLoader::prefetchImage};
  methodMap_["prefetchImageWithMetadata"] = {3, &QnxImageLoader::prefetchImageWithMetadata};
  methodMap_["queryCache"] = {1, &QnxImageLoader::queryCache};
}

facebook::jsi::Value QnxImageLoader::load(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count, bool withHeaders,
    bool prefetch) {
  auto& self = static_cast<QnxImageLoader&>(tm);
  facebook::react::AsyncPromise<folly::dynamic> promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  if (count < 1 || !args[0].isString() || args[0].asString(rt).utf8(rt).empty()) {
    promise.reject("ImageLoader requires a non-empty string URI");
    return result;
  }
  QtImageCache::Headers headers;
  if (withHeaders) {
    if (count < 2 || !args[1].isObject() || args[1].asObject(rt).isArray(rt) ||
        args[1].asObject(rt).isFunction(rt)) {
      promise.reject("getSizeWithHeaders requires a headers object");
      return result;
    }
    auto object = args[1].asObject(rt);
    auto names = object.getPropertyNames(rt);
    for (size_t i = 0; i < names.size(rt); ++i) {
      auto name = names.getValueAtIndex(rt, i).asString(rt);
      auto value = object.getProperty(rt, name);
      if (!value.isString()) {
        promise.reject("Image request header values must be strings");
        return result;
      }
      headers.emplace(QByteArray::fromStdString(name.utf8(rt)),
                      QByteArray::fromStdString(value.asString(rt).utf8(rt)));
    }
  }
  self.cache_.load(QString::fromStdString(args[0].asString(rt).utf8(rt)), headers,
                  [promise, withHeaders, prefetch](QSize size, QString error) mutable {
    if (!error.isEmpty()) {
      promise.reject(error.toStdString());
    } else if (prefetch) {
      promise.resolve(true);
    } else if (withHeaders) {
      promise.resolve(folly::dynamic::object("width", size.width())("height", size.height()));
    } else {
      promise.resolve(folly::dynamic::array(size.width(), size.height()));
    }
  });
  return result;
}

facebook::jsi::Value QnxImageLoader::getSize(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  return load(rt, tm, args, count, false, false);
}

facebook::jsi::Value QnxImageLoader::getSizeWithHeaders(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  return load(rt, tm, args, count, true, false);
}

facebook::jsi::Value QnxImageLoader::prefetchImage(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  return load(rt, tm, args, count, false, true);
}

facebook::jsi::Value QnxImageLoader::prefetchImageWithMetadata(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  // queryRootName/rootTag are tracing metadata, not HTTP headers or cache keys.
  return prefetchImage(rt, tm, args, count);
}

facebook::jsi::Value QnxImageLoader::queryCache(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  auto& self = static_cast<QnxImageLoader&>(tm);
  facebook::react::AsyncPromise<folly::dynamic> promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  if (count < 1 || !args[0].isObject() || !args[0].asObject(rt).isArray(rt)) {
    promise.reject("queryCache requires an array of URI strings");
    return result;
  }
  auto uris = args[0].asObject(rt).asArray(rt);
  folly::dynamic cached = folly::dynamic::object();
  for (size_t i = 0; i < uris.size(rt); ++i) {
    auto value = uris.getValueAtIndex(rt, i);
    if (!value.isString()) {
      promise.reject("queryCache requires an array of URI strings");
      return result;
    }
    auto uri = value.asString(rt).utf8(rt);
    if (self.cache_.contains(QString::fromStdString(uri))) cached[uri] = "memory";
  }
  promise.resolve(std::move(cached));
  return result;
}

facebook::jsi::Value QnxImageLoader::getConstants(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule&,
    const facebook::jsi::Value*, size_t) {
  return facebook::jsi::Object(rt);
}

}  // namespace rnqnx::runtime
