#include "ExpoQnxConstants.hpp"
#include "ExpoQnxConstantsConfig.hpp"
#include "ExpoQnxModuleRegistry.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QUuid>
#include <stdexcept>

namespace rnqnx::expo {
namespace {
facebook::jsi::Object createConstants(facebook::jsi::Runtime& runtime) {
  auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!application || application->thread() != QThread::currentThread()) {
    throw std::runtime_error("ExponentConstants requires the QGuiApplication owner thread");
  }
  namespace jsi = facebook::jsi;
  jsi::Object constants(runtime);
  constants.setProperty(runtime, "name", "ExponentConstants");
  constants.setProperty(runtime, "appOwnership", jsi::Value::null());
  constants.setProperty(runtime, "executionEnvironment", "bare");
  constants.setProperty(runtime, "expoVersion", jsi::Value::null());
  constants.setProperty(runtime, "expoRuntimeVersion", jsi::Value::null());
  constants.setProperty(runtime, "deviceYearClass", jsi::Value::null());
  constants.setProperty(runtime, "isHeadless", false);
  const auto dev = runtime.global().getProperty(runtime, "__DEV__");
  constants.setProperty(runtime, "debugMode", dev.isBool() && dev.getBool());
  // Process-session identity is random, never a persistent device identifier.
  static const auto session = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
  constants.setProperty(runtime, "sessionId", jsi::String::createFromUtf8(runtime, session));
  const auto fonts = QFontDatabase::families();
  jsi::Array systemFonts(runtime, static_cast<size_t>(fonts.size()));
  for (qsizetype i = 0; i < fonts.size(); ++i) {
    systemFonts.setValueAtIndex(runtime, static_cast<size_t>(i),
        jsi::String::createFromUtf8(runtime, fonts[i].toUtf8().toStdString()));
  }
  constants.setProperty(runtime, "systemFonts", std::move(systemFonts));
  const auto json = QByteArray::fromHex(RNQNX_EXPO_CONSTANTS_CONFIG_HEX);
  if (json.isEmpty()) {
    constants.setProperty(runtime, "manifest", jsi::Value::null());
  } else {
    // A reviewed public Expo config is embedded at configure time. No startup
    // filesystem/environment/network read and no native secret configuration.
    constants.setProperty(runtime, "manifest", jsi::Value::createFromJsonUtf8(runtime,
        reinterpret_cast<const uint8_t*>(json.constData()), static_cast<size_t>(json.size())));
    const auto config = QJsonDocument::fromJson(json).object();
    const auto scheme = config.value(QStringLiteral("scheme"));
    if (scheme.isString()) {
      constants.setProperty(runtime, "linkingUri",
          jsi::String::createFromUtf8(runtime, (scheme.toString() + QStringLiteral("://")).toStdString()));
    }
  }
  // No invented status bar height, device name or WebView user agent. Those
  // capabilities are absent until a real provider supplies them.
  return constants;
}
} // namespace
void registerConstantsModule() {
  ExpoQnxModuleRegistry::instance().registerModule("ExponentConstants", createConstants);
}
} // namespace rnqnx::expo
