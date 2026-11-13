#include "ExpoQnxInstaller.hpp"

#include "ExpoQnxModuleRegistry.hpp"
#include "ExpoQnxUuid.hpp"

#include <string>
#include <utility>

namespace rnqnx::expo {
namespace {

namespace jsi = facebook::jsi;

std::string& manifestStorage() {
  static std::string manifest;
  return manifest;
}

bool& debugStorage() {
  static bool debug = false;
  return debug;
}

std::string architecture() {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__x86_64__)
  return "x86_64";
#else
  return "unknown";
#endif
}

void registerBuiltins() {
  ExpoQnxModuleRegistry::instance().registerModule(
      "ExpoQnxPlatform",
      [](jsi::Runtime& runtime) {
        jsi::Object module(runtime);
        module.setProperty(runtime, "platform", "qnx");
        module.setProperty(runtime, "architecture", architecture());
        return module;
      });
}

jsi::Function hostFunction(
    jsi::Runtime& runtime,
    const char* name,
    unsigned int arguments,
    jsi::HostFunctionType body) {
  return jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, name), arguments, std::move(body));
}

jsi::Object coreVersion(jsi::Runtime& runtime) {
  // The version of the expo-modules-core JavaScript this host implements; the
  // package's peerDependencies pin the same version.
  jsi::Object version(runtime);
  version.setProperty(runtime, "version", "58.1.0");
  version.setProperty(runtime, "major", 58);
  version.setProperty(runtime, "minor", 1);
  version.setProperty(runtime, "patch", 0);
  return version;
}

} // namespace

void setAppManifestJson(std::string json) {
  manifestStorage() = std::move(json);
}

const std::string& appManifestJson() {
  return manifestStorage();
}

void setDebugBuild(bool debug) {
  debugStorage() = debug;
}

bool debugBuild() {
  return debugStorage();
}

void installExpoModulesQnx(jsi::Runtime& runtime) {
  registerBuiltins();

  jsi::Object expo(runtime);

  // A plain object, filled once per runtime. Expo's requireNativeModule reads
  // `globalThis.expo.modules[name]`; the QNX polyfill later gives every entry
  // NativeModule.prototype so addListener and friends work.
  jsi::Object modules(runtime);
  auto& registry = ExpoQnxModuleRegistry::instance();
  for (const auto& name : registry.names()) {
    modules.setProperty(runtime, name.c_str(), registry.createModule(runtime, name));
  }
  expo.setProperty(runtime, "modules", std::move(modules));

  expo.setProperty(runtime, "uuidv4",
      hostFunction(runtime, "uuidv4", 0,
          [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
            return jsi::Value(rt, jsi::String::createFromAscii(rt, uuidV4()));
          }));
  expo.setProperty(runtime, "uuidv5",
      hostFunction(runtime, "uuidv5", 2,
          [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) {
            if (count < 2 || !args[0].isString() || !args[1].isString()) {
              throw jsi::JSError(rt, "uuidv5(name, namespace) takes two strings");
            }
            auto result = uuidV5(args[0].getString(rt).utf8(rt), args[1].getString(rt).utf8(rt));
            if (!result) {
              throw jsi::JSError(rt, "uuidv5 namespace must be a UUID string");
            }
            return jsi::Value(rt, jsi::String::createFromAscii(rt, *result));
          }));
  // Expo views declare their props through getViewConfig on iOS and Android:
  // requireNativeView builds the host component from the result. A view
  // registers its config with ExpoQnxModuleRegistry::registerViewConfig; an
  // unregistered view yields null, and requireNativeView then warns and builds
  // a config without validAttributes or directEventTypes.
  expo.setProperty(runtime, "getViewConfig",
      hostFunction(runtime, "getViewConfig", 2,
          [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value* args, size_t count) -> jsi::Value {
            if (count < 1 || !args[0].isString()) {
              throw jsi::JSError(rt, "getViewConfig(moduleName, viewName?) needs a module name");
            }
            const std::string moduleName = args[0].getString(rt).utf8(rt);
            std::string viewName;
            if (count > 1 && args[1].isString()) {
              viewName = args[1].getString(rt).utf8(rt);
            }
            ExpoViewConfig config;
            if (!ExpoQnxModuleRegistry::instance().findViewConfig(moduleName, viewName, config)) {
              return jsi::Value::null();
            }
            jsi::Object validAttributes(rt);
            for (const auto& prop : config.props) {
              validAttributes.setProperty(rt, prop.c_str(), true);
            }
            jsi::Object directEventTypes(rt);
            for (const auto& handler : config.events) {
              // Codegen'd view configs list the handler as a valid attribute too.
              validAttributes.setProperty(rt, handler.c_str(), true);
              if (handler.size() < 3 || handler.compare(0, 2, "on") != 0) {
                throw jsi::JSError(rt, "Expo view event handler must start with 'on': " + handler);
              }
              jsi::Object type(rt);
              type.setProperty(rt, "registrationName", jsi::String::createFromUtf8(rt, handler));
              directEventTypes.setProperty(rt, ("top" + handler.substr(2)).c_str(), std::move(type));
            }
            jsi::Object result(rt);
            result.setProperty(rt, "validAttributes", std::move(validAttributes));
            result.setProperty(rt, "directEventTypes", std::move(directEventTypes));
            return jsi::Value(std::move(result));
          }));
  expo.setProperty(runtime, "reloadAppAsync",
      hostFunction(runtime, "reloadAppAsync", 1,
          [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) -> jsi::Value {
            throw jsi::JSError(
                rt, "reloadAppAsync is not available on QNX; reload from the dev menu");
          }));
  expo.setProperty(runtime, "expoModulesCoreVersion", coreVersion(runtime));
  expo.setProperty(runtime, "isRunningOnQuest", false);

  // Expo Modules Core's native bootstrap first checks globalThis.expo. By
  // installing this before the JS bundle runs, QNX follows the native-startup
  // path and does not need an Android-style ExpoModulesCore bridge module.
  runtime.global().setProperty(runtime, "expo", std::move(expo));
}

} // namespace rnqnx::expo
