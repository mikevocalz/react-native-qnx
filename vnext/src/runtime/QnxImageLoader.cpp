#include "react-native-qnx/runtime/QnxImageLoader.h"

#include <react/bridging/Dynamic.h>
#include <react/bridging/Promise.h>

#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"

#include <algorithm>
#include <cstdint>

namespace rnqnx::runtime {

namespace {
// "No limit" for the disk cache: far beyond any disk this runs on.
constexpr std::int64_t kUnlimitedDiskBytes = std::int64_t{1} << 50;
}  // namespace

QnxImageLoader::QnxImageLoader(
    std::shared_ptr<facebook::react::CallInvoker> jsInvoker)
    : facebook::react::TurboModule(kModuleName, std::move(jsInvoker)) {
  registerMethods();
  // Forward every picture that loads to expo-image's `imageLoaded` module
  // event. The listener holds the invoker, not this module.
  loadedSubscription_ = views::ImageLoadedNotifier::shared().subscribe(
      [invoker = jsInvoker_](const views::ImageLoadedEvent& event) {
        invoker->invokeAsync([event](facebook::jsi::Runtime& rt) { emitImageLoaded(rt, event); });
      });
}

QnxImageLoader::~QnxImageLoader() { views::ImageLoadedNotifier::shared().unsubscribe(loadedSubscription_); }

void QnxImageLoader::registerMethods() {
  methodMap_["getConstants"] = {0, &QnxImageLoader::getConstants};
  methodMap_["getSize"] = {1, &QnxImageLoader::getSize};
  methodMap_["getSizeWithHeaders"] = {2, &QnxImageLoader::getSizeWithHeaders};
  methodMap_["prefetchImage"] = {1, &QnxImageLoader::prefetchImage};
  methodMap_["prefetchImageWithMetadata"] = {3, &QnxImageLoader::prefetchImageWithMetadata};
  methodMap_["queryCache"] = {1, &QnxImageLoader::queryCache};
  methodMap_["prefetchImageWithOptions"] = {2, &QnxImageLoader::prefetchImageWithOptions};
  methodMap_["clearMemoryCache"] = {0, &QnxImageLoader::clearMemoryCache};
  methodMap_["clearDiskCache"] = {0, &QnxImageLoader::clearDiskCache};
  methodMap_["getCachePath"] = {1, &QnxImageLoader::getCachePath};
  methodMap_["configureCache"] = {1, &QnxImageLoader::configureCache};
  methodMap_["loadImage"] = {2, &QnxImageLoader::loadImage};
  methodMap_["releaseImage"] = {1, &QnxImageLoader::releaseImage};
  methodMap_["generateBlurhash"] = {3, &QnxImageLoader::generateBlurhash};
  methodMap_["generateThumbhash"] = {1, &QnxImageLoader::generateThumbhash};
  methodMap_["writeToCache"] = {2, &QnxImageLoader::writeToCache};
  methodMap_["readFromCache"] = {1, &QnxImageLoader::readFromCache};
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

namespace {

/// The request `expo-image` describes: `{ headers?, cachePolicy?, cacheKey? }`
/// next to a URI. Returns an error text, empty when the request is valid.
std::string readOptions(facebook::jsi::Runtime& rt, const facebook::jsi::Value* args, size_t count,
                        views::ImageRequest& request) {
  if (count < 1 || !args[0].isString() || args[0].asString(rt).utf8(rt).empty()) {
    return "ImageLoader requires a non-empty string URI";
  }
  request.uri = QString::fromStdString(args[0].asString(rt).utf8(rt));
  if (count < 2 || !args[1].isObject()) return {};
  auto options = args[1].asObject(rt);
  if (auto policy = options.getProperty(rt, "cachePolicy"); policy.isString()) {
    request.policy = views::parseCachePolicy(QString::fromStdString(policy.asString(rt).utf8(rt)));
  }
  if (auto key = options.getProperty(rt, "cacheKey"); key.isString()) {
    request.cacheKey = QString::fromStdString(key.asString(rt).utf8(rt));
  }
  if (auto headers = options.getProperty(rt, "headers"); headers.isObject()) {
    auto object = headers.asObject(rt);
    auto names = object.getPropertyNames(rt);
    for (size_t i = 0; i < names.size(rt); ++i) {
      auto name = names.getValueAtIndex(rt, i).asString(rt);
      auto value = object.getProperty(rt, name);
      if (!value.isString()) return "Image request header values must be strings";
      request.headers.emplace(QByteArray::fromStdString(name.utf8(rt)),
                              QByteArray::fromStdString(value.asString(rt).utf8(rt)));
    }
  }
  return {};
}

}  // namespace

facebook::jsi::Value QnxImageLoader::prefetchImageWithOptions(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  auto& self = static_cast<QnxImageLoader&>(tm);
  facebook::react::AsyncPromise<folly::dynamic> promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  views::ImageRequest request;
  if (const auto error = readOptions(rt, args, count, request); !error.empty()) {
    promise.reject(error);
    return result;
  }
  // A prefetch with a disk policy fills the disk cache as well as memory.
  self.cache_.load(request, /*store=*/true, [promise](QSize, QString error) mutable {
    if (!error.isEmpty()) promise.reject(error.toStdString());
    else promise.resolve(true);
  });
  return result;
}

facebook::jsi::Value QnxImageLoader::clearMemoryCache(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value*, size_t) {
  auto& self = static_cast<QnxImageLoader&>(tm);
  facebook::react::AsyncPromise<folly::dynamic> promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  self.cache_.clearMemory();
  promise.resolve(true);
  return result;
}

facebook::jsi::Value QnxImageLoader::clearDiskCache(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value*, size_t) {
  auto& self = static_cast<QnxImageLoader&>(tm);
  facebook::react::AsyncPromise<folly::dynamic> promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  views::ImageDiskCache::shared().clear();
  promise.resolve(true);
  return result;
}

facebook::jsi::Value QnxImageLoader::getCachePath(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args, size_t count) {
  auto& self = static_cast<QnxImageLoader&>(tm);
  facebook::react::AsyncPromise<folly::dynamic> promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  if (count < 1 || !args[0].isString() || args[0].asString(rt).utf8(rt).empty()) {
    promise.reject("getCachePath requires a cache key or URL");
    return result;
  }
  // expo-image's getCachePathAsync takes the `cacheKey` a source named, or the
  // URL when it named none. Only an entry that exists has a path; null otherwise.
  const QString given = QString::fromStdString(args[0].asString(rt).utf8(rt));
  auto& disk = views::ImageDiskCache::shared();
  for (const QString& key : {QStringLiteral("key:") + given, given}) {
    if (disk.contains(key)) {
      promise.resolve(disk.pathFor(key).toStdString());
      return result;
    }
  }
  promise.resolve(folly::dynamic(nullptr));
  return result;
}

// expo-image's configureCache({ maxDiskSize, maxMemoryCost, maxMemoryCount }):
// bytes for the disk cache, bytes of decoded pictures for the memory cache and
// a picture count. Zero (the documented default) means no limit for the disk
// and count and leaves the memory ceiling alone.
facebook::jsi::Value QnxImageLoader::configureCache(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule&,
    const facebook::jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject()) {
    throw facebook::jsi::JSError(rt, "configureCache requires a config object");
  }
  const auto config = args[0].asObject(rt);
  const auto number = [&](const char* name) {
    const auto value = config.getProperty(rt, name);
    return value.isNumber() ? value.getNumber() : 0.0;
  };
  if (config.hasProperty(rt, "maxDiskSize")) {
    const double bytes = number("maxDiskSize");
    views::ImageDiskCache::shared().setBudgetBytes(
        bytes > 0 ? static_cast<std::int64_t>(bytes) : kUnlimitedDiskBytes);
  }
  const double memoryBytes = number("maxMemoryCost");
  views::DecodedImageCache::shared().configure(
      memoryBytes > 0 ? static_cast<int>(std::min(memoryBytes / 1024.0, 2.0e9)) : 0,
      static_cast<int>(std::min(number("maxMemoryCount"), 2.0e9)));
  return facebook::jsi::Value::undefined();
}

facebook::jsi::Value QnxImageLoader::getConstants(
    facebook::jsi::Runtime& rt, facebook::react::TurboModule&,
    const facebook::jsi::Value*, size_t) {
  return facebook::jsi::Object(rt);
}

}  // namespace rnqnx::runtime
