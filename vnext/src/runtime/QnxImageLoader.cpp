#include "react-native-qnx/runtime/QnxImageLoader.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QUrl>

#include <optional>
#include <string>

namespace rnqnx::runtime {

namespace {

std::optional<QSize> loadImageSize(const std::string& uri, std::string& error) {
  if (uri.starts_with("data:")) {
    const auto comma = uri.find(',');
    if (comma == std::string::npos) {
      error = "Malformed data URI";
      return std::nullopt;
    }
    const auto base64 = uri.substr(comma + 1);
    QByteArray bytes = QByteArray::fromBase64(
        QByteArray::fromRawData(base64.data(), base64.size()));
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    QSize size = reader.size();
    if (size.isValid()) {
      return size;
    }
    QImage image;
    if (image.loadFromData(bytes)) {
      return image.size();
    }
    error = "Could not decode data URI";
    return std::nullopt;
  }

  if (uri.starts_with("file://") || uri.starts_with("/")) {
    const QString path =
        uri.starts_with("file://")
            ? QUrl(QString::fromStdString(uri)).toLocalFile()
            : QString::fromStdString(uri);
    QImageReader reader(path);
    QSize size = reader.size();
    if (size.isValid()) {
      return size;
    }
    QImage image(path);
    if (!image.isNull()) {
      return image.size();
    }
    error = "Could not load image";
    return std::nullopt;
  }

  if (uri.starts_with("qrc:/")) {
    QImage image(QString::fromStdString(uri));
    if (!image.isNull()) {
      return image.size();
    }
    error = "Could not load qrc image";
    return std::nullopt;
  }

  error = "Unsupported image URI scheme";
  return std::nullopt;
}

}  // namespace

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
  methodMap_["prefetchImageWithMetadata"] = {
      3,
      &QnxImageLoader::prefetchImageWithMetadata};
  methodMap_["queryCache"] = {1, &QnxImageLoader::queryCache};
}

facebook::jsi::Value QnxImageLoader::getSize(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args,
    size_t count) {
  (void)tm;
  if (count < 1 || !args[0].isString()) {
    auto Error = rt.global().getPropertyAsObject(rt, "Error");
    auto error =
        Error.asFunction(rt).callAsConstructor(rt, facebook::jsi::String::createFromUtf8(rt, "getSize requires a string uri"));
    return error;
  }

  auto Promise = rt.global().getPropertyAsObject(rt, "Promise").asFunction(rt);
  std::string uri = args[0].getString(rt).utf8(rt);

  auto executor = facebook::jsi::Function::createFromHostFunction(
      rt,
      facebook::jsi::PropNameID::forAscii(rt, "executor"),
      2,
      [uri = std::move(uri)](
          facebook::jsi::Runtime& r,
          const facebook::jsi::Value&,
          const facebook::jsi::Value* args,
          size_t) {
        auto resolve = args[0].asObject(r).asFunction(r);
        auto reject = args[1].asObject(r).asFunction(r);

        std::string error;
        auto size = loadImageSize(uri, error);
        if (!size) {
          auto Error = r.global().getPropertyAsObject(r, "Error");
          auto err = Error.asFunction(r).callAsConstructor(
              r,
              facebook::jsi::String::createFromUtf8(r, error));
          facebook::jsi::Value errValue(r, err);
          reject.call(r, static_cast<const facebook::jsi::Value*>(&errValue), static_cast<size_t>(1));
          return facebook::jsi::Value::undefined();
        }

        auto array = facebook::jsi::Array(r, 2);
        array.setValueAtIndex(r, 0, static_cast<double>(size->width()));
        array.setValueAtIndex(r, 1, static_cast<double>(size->height()));
        facebook::jsi::Value value(r, array);
        resolve.call(r, static_cast<const facebook::jsi::Value*>(&value), static_cast<size_t>(1));
        return facebook::jsi::Value::undefined();
      });

  return Promise.callAsConstructor(rt, executor);
}

facebook::jsi::Value QnxImageLoader::getSizeWithHeaders(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args,
    size_t count) {
  (void)tm;
  (void)count;
  if (count < 2 || !args[0].isString()) {
    auto Error = rt.global().getPropertyAsObject(rt, "Error");
    auto error =
        Error.asFunction(rt).callAsConstructor(rt, facebook::jsi::String::createFromUtf8(rt, "getSizeWithHeaders requires a string uri"));
    return error;
  }

  auto Promise = rt.global().getPropertyAsObject(rt, "Promise").asFunction(rt);
  std::string uri = args[0].getString(rt).utf8(rt);

  auto executor = facebook::jsi::Function::createFromHostFunction(
      rt,
      facebook::jsi::PropNameID::forAscii(rt, "executor"),
      2,
      [uri = std::move(uri)](
          facebook::jsi::Runtime& r,
          const facebook::jsi::Value&,
          const facebook::jsi::Value* args,
          size_t) {
        auto resolve = args[0].asObject(r).asFunction(r);
        auto reject = args[1].asObject(r).asFunction(r);

        std::string error;
        auto size = loadImageSize(uri, error);
        if (!size) {
          auto Error = r.global().getPropertyAsObject(r, "Error");
          auto err = Error.asFunction(r).callAsConstructor(
              r,
              facebook::jsi::String::createFromUtf8(r, error));
          facebook::jsi::Value errValue(r, err);
          reject.call(r, static_cast<const facebook::jsi::Value*>(&errValue), static_cast<size_t>(1));
          return facebook::jsi::Value::undefined();
        }

        facebook::jsi::Object result(r);
        result.setProperty(r, "width", static_cast<double>(size->width()));
        result.setProperty(r, "height", static_cast<double>(size->height()));
        facebook::jsi::Value value(r, result);
        resolve.call(r, static_cast<const facebook::jsi::Value*>(&value), static_cast<size_t>(1));
        return facebook::jsi::Value::undefined();
      });

  return Promise.callAsConstructor(rt, executor);
}

facebook::jsi::Value QnxImageLoader::prefetchImage(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args,
    size_t count) {
  (void)tm;
  (void)args;
  (void)count;

  auto Promise = rt.global().getPropertyAsObject(rt, "Promise").asFunction(rt);
  auto executor = facebook::jsi::Function::createFromHostFunction(
      rt,
      facebook::jsi::PropNameID::forAscii(rt, "executor"),
      2,
      [](facebook::jsi::Runtime& r,
         const facebook::jsi::Value&,
         const facebook::jsi::Value* args,
         size_t) {
        auto resolve = args[0].asObject(r).asFunction(r);
        resolve.call(r, facebook::jsi::Value(true));
        return facebook::jsi::Value::undefined();
      });

  return Promise.callAsConstructor(rt, executor);
}

facebook::jsi::Value QnxImageLoader::prefetchImageWithMetadata(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args,
    size_t count) {
  return prefetchImage(rt, tm, args, count);
}

facebook::jsi::Value QnxImageLoader::queryCache(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args,
    size_t count) {
  (void)tm;
  (void)args;
  (void)count;

  auto Promise = rt.global().getPropertyAsObject(rt, "Promise").asFunction(rt);
  auto executor = facebook::jsi::Function::createFromHostFunction(
      rt,
      facebook::jsi::PropNameID::forAscii(rt, "executor"),
      2,
      [](facebook::jsi::Runtime& r,
         const facebook::jsi::Value&,
         const facebook::jsi::Value* args,
         size_t) {
        auto resolve = args[0].asObject(r).asFunction(r);
        facebook::jsi::Object value(r);
        resolve.call(r, facebook::jsi::Value(r, value));
        return facebook::jsi::Value::undefined();
      });

  return Promise.callAsConstructor(rt, executor);
}

facebook::jsi::Value QnxImageLoader::getConstants(
    facebook::jsi::Runtime& rt,
    facebook::react::TurboModule& tm,
    const facebook::jsi::Value* args,
    size_t count) {
  (void)tm;
  (void)args;
  (void)count;
  return facebook::jsi::Object(rt);
}

}  // namespace rnqnx::runtime
