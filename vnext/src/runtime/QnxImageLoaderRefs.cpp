// The ImageRef half of the ImageLoader module: what @rn-qnx/expo-image-qnx
// needs for expo-image's loadAsync, generateBlurhashAsync,
// generateThumbhashAsync, writeToCacheAsync and readFromCacheAsync. Every
// function returns a Promise and takes the same "source" argument: the id of an
// ImageRef, a URI string, or an expo-image source object.
#include "react-native-qnx/runtime/QnxImageLoader.h"

#include <react/bridging/Dynamic.h>
#include <react/bridging/Promise.h>

#include "react-native-qnx/views/DecodedImageCache.h"
#include "react-native-qnx/views/image/BlurHash.h"
#include "react-native-qnx/views/image/BlurHashEncoder.h"
#include "react-native-qnx/views/image/ImageDecode.h"
#include "react-native-qnx/views/image/ImageDiskCache.h"
#include "react-native-qnx/views/image/ImageEffects.h"
#include "react-native-qnx/views/image/ImageLoadedNotifier.h"
#include "react-native-qnx/views/image/ImageRefRegistry.h"
#include "react-native-qnx/views/image/ThumbHash.h"
#include "react-native-qnx/views/image/ThumbHashEncoder.h"

#include <QBuffer>
#include <QUrl>

#include <algorithm>
#include <cstdint>
#include <functional>

namespace rnqnx::runtime {
namespace {

namespace jsi = facebook::jsi;
using Promise = facebook::react::AsyncPromise<folly::dynamic>;

/// What a source argument names.
struct SourceSpec {
  std::uint32_t refId{0};
  views::ImageRequest request;
  int width{0};
  int height{0};
};

/// A decoded source. \a encoded holds the original bytes when the source was
/// fetched, so a cache write can keep an animated image whole.
struct Picture {
  QImage image;
  QString mediaType;
  QByteArray encoded;
  bool animated{false};
  QString error;
};

using PictureDone = std::function<void(Picture)>;

std::string readHeaders(jsi::Runtime& rt, const jsi::Object& object, views::ImageHeaders& out) {
  auto names = object.getPropertyNames(rt);
  for (size_t i = 0; i < names.size(rt); ++i) {
    auto name = names.getValueAtIndex(rt, i).asString(rt);
    auto value = object.getProperty(rt, name);
    if (!value.isString()) return "Image request header values must be strings";
    out.emplace(QByteArray::fromStdString(name.utf8(rt)),
                QByteArray::fromStdString(value.asString(rt).utf8(rt)));
  }
  return {};
}

/// Reads an ImageRef id, a URI string or a source object. Returns an error
/// text, empty on success.
std::string readSource(jsi::Runtime& rt, const jsi::Value& arg, SourceSpec& spec) {
  if (arg.isNumber()) {
    spec.refId = static_cast<std::uint32_t>(arg.getNumber());
    return spec.refId == 0 ? "Invalid ImageRef" : std::string();
  }
  if (arg.isString()) {
    spec.request.uri = QString::fromStdString(arg.getString(rt).utf8(rt));
    return spec.request.uri.isEmpty() ? "The image source has no uri" : std::string();
  }
  if (!arg.isObject()) return "The image source must be a URI, an ImageSource or an ImageRef";
  const jsi::Object object = arg.getObject(rt);
  const auto text = [&](const char* name) {
    auto value = object.getProperty(rt, name);
    return value.isString() ? QString::fromStdString(value.asString(rt).utf8(rt)) : QString();
  };
  spec.request.uri = text("uri");
  spec.request.cacheKey = text("cacheKey");
  if (auto width = object.getProperty(rt, "width"); width.isNumber()) spec.width = static_cast<int>(width.getNumber());
  if (auto height = object.getProperty(rt, "height"); height.isNumber()) spec.height = static_cast<int>(height.getNumber());
  if (auto headers = object.getProperty(rt, "headers"); headers.isObject()) {
    if (auto error = readHeaders(rt, headers.asObject(rt), spec.request.headers); !error.empty()) return error;
  }
  return spec.request.uri.isEmpty() ? "The image source has no uri" : std::string();
}

Picture failure(const QString& message) {
  Picture picture;
  picture.error = message;
  return picture;
}

/// expo-image writes hash sources as `blurhash:/<hash>` and `thumbhash:/<hash>`,
/// percent-encoded; a thumbhash's slashes travel as backslashes.
Picture hashPicture(const SourceSpec& spec) {
  const QString uri = spec.request.uri;
  Picture picture;
  if (uri.startsWith(QLatin1String("blurhash:/"))) {
    const QString hash = QUrl::fromPercentEncoding(uri.mid(10).toUtf8());
    picture.image = views::decodeBlurHash(hash, spec.width > 0 ? std::min(spec.width, 128) : 16,
                                          spec.height > 0 ? std::min(spec.height, 128) : 16);
  } else {
    QString hash = QUrl::fromPercentEncoding(uri.mid(11).toUtf8());
    hash.replace(QLatin1Char('\\'), QLatin1Char('/'));
    picture.image = views::decodeThumbHash(hash);
  }
  if (picture.image.isNull()) picture.error = QStringLiteral("Invalid blurhash or thumbhash");
  return picture;
}

void obtainPicture(views::QtImageFetcher& fetcher, const SourceSpec& spec, PictureDone done) {
  if (spec.refId != 0) {
    const auto entry = views::ImageRefRegistry::shared().find(spec.refId);
    if (entry.image.isNull()) {
      done(failure(QStringLiteral("The ImageRef was released")));
      return;
    }
    Picture picture;
    picture.image = entry.image;
    picture.mediaType = entry.mediaType;
    done(std::move(picture));
    return;
  }
  if (spec.request.uri.startsWith(QLatin1String("blurhash:/")) ||
      spec.request.uri.startsWith(QLatin1String("thumbhash:/"))) {
    done(hashPicture(spec));
    return;
  }
  fetcher.fetch(spec.request, {}, [done = std::move(done)](views::FetchResult result) {
    if (!result.error.isEmpty()) {
      done(failure(result.error));
      return;
    }
    views::DecodedImage decoded =
        views::decodeImage(result.bytes, QSize(), false, views::ContentFit::Cover);
    if (!decoded.error.isEmpty()) {
      done(failure(decoded.error));
      return;
    }
    Picture picture;
    picture.image = std::move(decoded.first);
    picture.mediaType = decoded.mediaType;
    picture.animated = decoded.animated;
    picture.encoded = std::move(result.bytes);
    done(std::move(picture));
  });
}

QnxImageLoader& loaderOf(facebook::react::TurboModule& tm) { return static_cast<QnxImageLoader&>(tm); }

folly::dynamic describe(std::uint32_t id, const QImage& image, const QString& mediaType) {
  return folly::dynamic::object("id", id)("width", image.width())("height", image.height())("scale", 1)(
      "mediaType", mediaType.isEmpty() ? folly::dynamic(nullptr) : folly::dynamic(mediaType.toStdString()))(
      "isAnimated", false);
}

std::uint32_t retain(const Picture& picture) {
  views::ImageRefEntry entry;
  entry.image = picture.image;
  entry.mediaType = picture.mediaType;
  return views::ImageRefRegistry::shared().add(std::move(entry));
}

/// The storage key a source's `cacheKey` maps to (ImageRequest::storageKey).
QString keyed(const QString& cacheKey) { return QStringLiteral("key:") + cacheKey; }

QImage fitWithin(QImage image, int maxWidth, int maxHeight) {
  const int limitWidth = maxWidth > 0 ? maxWidth : image.width();
  const int limitHeight = maxHeight > 0 ? maxHeight : image.height();
  if (image.width() <= limitWidth && image.height() <= limitHeight) return image;
  return image.scaled(limitWidth, limitHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

}  // namespace

void emitImageLoaded(jsi::Runtime& rt, const views::ImageLoadedEvent& event) {
  auto expo = rt.global().getProperty(rt, "expo");
  if (!expo.isObject()) return;
  auto modules = expo.asObject(rt).getProperty(rt, "modules");
  if (!modules.isObject()) return;
  auto module = modules.asObject(rt).getProperty(rt, "ExpoImage");
  if (!module.isObject()) return;
  auto target = module.asObject(rt);
  auto emit = target.getProperty(rt, "emit");
  auto count = target.getProperty(rt, "listenerCount");
  if (!emit.isObject() || !emit.asObject(rt).isFunction(rt) || !count.isObject() ||
      !count.asObject(rt).isFunction(rt)) {
    return;
  }
  const auto name = jsi::String::createFromAscii(rt, "imageLoaded");
  if (count.asObject(rt).asFunction(rt).callWithThis(rt, target, name).asNumber() < 1) return;
  jsi::Object payload(rt);
  payload.setProperty(rt, "url", jsi::String::createFromUtf8(rt, event.url.toStdString()));
  payload.setProperty(rt, "width", event.width);
  payload.setProperty(rt, "height", event.height);
  emit.asObject(rt).asFunction(rt).callWithThis(rt, target, name, payload);
}

// loadImage(source, { maxWidth?, maxHeight?, tintColor? })
//   -> { id, width, height, scale, mediaType, isAnimated }
jsi::Value QnxImageLoader::loadImage(jsi::Runtime& rt, facebook::react::TurboModule& tm,
                                     const jsi::Value* args, size_t count) {
  auto& self = loaderOf(tm);
  Promise promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  SourceSpec spec;
  if (count < 1) {
    promise.reject("loadAsync needs an image source");
    return result;
  }
  if (const auto error = readSource(rt, args[0], spec); !error.empty()) {
    promise.reject(error);
    return result;
  }
  spec.request.policy = views::ImageCachePolicy::MemoryDisk;
  int maxWidth = 0, maxHeight = 0;
  bool tinted = false;
  QColor tint;
  if (count > 1 && args[1].isObject()) {
    const auto options = args[1].getObject(rt);
    if (auto value = options.getProperty(rt, "maxWidth"); value.isNumber()) maxWidth = static_cast<int>(value.getNumber());
    if (auto value = options.getProperty(rt, "maxHeight"); value.isNumber()) maxHeight = static_cast<int>(value.getNumber());
    if (auto value = options.getProperty(rt, "tintColor"); value.isNumber()) {
      // processColor's 0xAARRGGBB, as a signed 32-bit number.
      tint = QColor::fromRgba(static_cast<QRgb>(static_cast<std::uint32_t>(static_cast<std::int64_t>(value.getNumber()))));
      tinted = true;
    }
  }
  const QString uri = spec.refId != 0 ? QString() : spec.request.uri;
  obtainPicture(self.fetcher_, spec, [promise, uri, maxWidth, maxHeight, tinted, tint](Picture picture) mutable {
    if (!picture.error.isEmpty()) {
      promise.reject(picture.error.toStdString());
      return;
    }
    picture.image = fitWithin(std::move(picture.image), maxWidth, maxHeight);
    if (tinted) picture.image = views::tintImage(picture.image, tint);
    views::ImageLoadedNotifier::shared().notify({uri, picture.image.width(), picture.image.height()});
    promise.resolve(describe(retain(picture), picture.image, picture.mediaType));
  });
  return result;
}

// releaseImage(id) -> boolean. The ImageRef's release().
jsi::Value QnxImageLoader::releaseImage(jsi::Runtime& rt, facebook::react::TurboModule&,
                                        const jsi::Value* args, size_t count) {
  const bool released = count > 0 && args[0].isNumber() &&
                        views::ImageRefRegistry::shared().release(static_cast<std::uint32_t>(args[0].getNumber()));
  (void)rt;
  return jsi::Value(released);
}

// generateBlurhash(source, componentsX, componentsY) -> string | null
jsi::Value QnxImageLoader::generateBlurhash(jsi::Runtime& rt, facebook::react::TurboModule& tm,
                                            const jsi::Value* args, size_t count) {
  auto& self = loaderOf(tm);
  Promise promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  SourceSpec spec;
  if (count < 3 || !args[1].isNumber() || !args[2].isNumber()) {
    promise.reject("generateBlurhashAsync needs a source and numberOfComponents");
    return result;
  }
  if (const auto error = readSource(rt, args[0], spec); !error.empty()) {
    promise.reject(error);
    return result;
  }
  const int componentsX = static_cast<int>(args[1].getNumber());
  const int componentsY = static_cast<int>(args[2].getNumber());
  obtainPicture(self.fetcher_, spec, [promise, componentsX, componentsY](Picture picture) mutable {
    if (!picture.error.isEmpty()) {
      promise.reject(picture.error.toStdString());
      return;
    }
    const QString hash = views::encodeBlurHash(picture.image, componentsX, componentsY);
    promise.resolve(hash.isEmpty() ? folly::dynamic(nullptr) : folly::dynamic(hash.toStdString()));
  });
  return result;
}

// generateThumbhash(source) -> base64 string
jsi::Value QnxImageLoader::generateThumbhash(jsi::Runtime& rt, facebook::react::TurboModule& tm,
                                             const jsi::Value* args, size_t count) {
  auto& self = loaderOf(tm);
  Promise promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  SourceSpec spec;
  if (count < 1) {
    promise.reject("generateThumbhashAsync needs an image source");
    return result;
  }
  if (const auto error = readSource(rt, args[0], spec); !error.empty()) {
    promise.reject(error);
    return result;
  }
  obtainPicture(self.fetcher_, spec, [promise](Picture picture) mutable {
    if (!picture.error.isEmpty()) {
      promise.reject(picture.error.toStdString());
      return;
    }
    promise.resolve(views::encodeThumbHash(picture.image).toBase64().toStdString());
  });
  return result;
}

// writeToCache(source, cacheKey): stores the image under the key in the disk
// cache (the file's own bytes for a URI, a PNG for an ImageRef) and the decoded
// cache, so a source with that `cacheKey` is served from them.
jsi::Value QnxImageLoader::writeToCache(jsi::Runtime& rt, facebook::react::TurboModule& tm,
                                        const jsi::Value* args, size_t count) {
  auto& self = loaderOf(tm);
  Promise promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  SourceSpec spec;
  if (count < 2 || !args[1].isString() || args[1].asString(rt).utf8(rt).empty()) {
    promise.reject("writeToCacheAsync needs a source and a cacheKey");
    return result;
  }
  if (const auto error = readSource(rt, args[0], spec); !error.empty()) {
    promise.reject(error);
    return result;
  }
  const QString key = keyed(QString::fromStdString(args[1].asString(rt).utf8(rt)));
  obtainPicture(self.fetcher_, spec, [promise, key](Picture picture) mutable {
    if (!picture.error.isEmpty()) {
      promise.reject(picture.error.toStdString());
      return;
    }
    QByteArray bytes = picture.encoded;
    if (bytes.isEmpty()) {
      QBuffer buffer(&bytes);
      buffer.open(QIODevice::WriteOnly);
      if (!picture.image.save(&buffer, "PNG")) {
        promise.reject("Could not encode the image for the cache");
        return;
      }
    }
    if (!views::ImageDiskCache::shared().write(key, bytes)) {
      promise.reject("The image is larger than the disk cache");
      return;
    }
    views::DecodedImageCache::shared().insert(key, picture.image, picture.mediaType,
        picture.animated ? picture.encoded : QByteArray{});
    promise.resolve(folly::dynamic(nullptr));
  });
  return result;
}

// readFromCache(cacheKey) -> ImageRef descriptor | null. Looks for the key a
// source's `cacheKey` names, then for the URL itself.
jsi::Value QnxImageLoader::readFromCache(jsi::Runtime& rt, facebook::react::TurboModule& tm,
                                         const jsi::Value* args, size_t count) {
  auto& self = loaderOf(tm);
  Promise promise(rt, self.jsInvoker_);
  auto result = promise.get(rt);
  if (count < 1 || !args[0].isString() || args[0].asString(rt).utf8(rt).empty()) {
    promise.reject("readFromCacheAsync needs a cache key");
    return result;
  }
  const QString given = QString::fromStdString(args[0].asString(rt).utf8(rt));
  auto& memory = views::DecodedImageCache::shared();
  auto& disk = views::ImageDiskCache::shared();
  for (const QString& key : {keyed(given), given}) {
    Picture picture;
    const auto cached = memory.entry(views::DecodedImageCache::key(key));
    picture.image = cached.image;
    picture.mediaType = cached.mediaType;
    if (picture.image.isNull()) {
      const QByteArray bytes = disk.read(key);
      if (bytes.isEmpty()) continue;
      views::DecodedImage decoded = views::decodeImage(bytes, QSize(), false, views::ContentFit::Cover);
      if (!decoded.error.isEmpty()) continue;
      picture.image = std::move(decoded.first);
      picture.mediaType = decoded.mediaType;
    }
    views::ImageLoadedNotifier::shared().notify({given, picture.image.width(), picture.image.height()});
    promise.resolve(describe(retain(picture), picture.image, picture.mediaType));
    return result;
  }
  promise.resolve(folly::dynamic(nullptr));
  return result;
}

}  // namespace rnqnx::runtime
