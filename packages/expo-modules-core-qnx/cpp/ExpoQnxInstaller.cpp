#include "ExpoQnxInstaller.hpp"

#include "ExpoQnxModuleRegistry.hpp"
#if RNQNX_ENABLE_EXPO_APPLICATION
#include "ExpoQnxApplication.hpp"
#endif
#if RNQNX_ENABLE_EXPO_CONSTANTS
#include "ExpoQnxConstants.hpp"
#endif
#include "ExpoQnxModulesHostObject.hpp"

#include <memory>
#include <string>

#if RNQNX_HAS_EXPO_AUTOLINKING
void rnqnxRegisterExpoAutolinkedModules(rnqnx::expo::ExpoQnxModuleRegistry& registry);
#endif

namespace rnqnx::expo {
namespace {

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
      [](facebook::jsi::Runtime& runtime) {
        facebook::jsi::Object module(runtime);
        module.setProperty(runtime, "platform", "qnx");
        module.setProperty(runtime, "architecture", architecture());
        return module;
      });
}

} // namespace

void installExpoModulesQnx(facebook::jsi::Runtime& runtime) {
  registerBuiltins();
#if RNQNX_ENABLE_EXPO_CONSTANTS
  registerConstantsModule();
#endif
#if RNQNX_HAS_EXPO_AUTOLINKING
  rnqnxRegisterExpoAutolinkedModules(ExpoQnxModuleRegistry::instance());
#endif
#if RNQNX_ENABLE_EXPO_APPLICATION
  registerApplicationModule();
#endif

  facebook::jsi::Object expo(runtime);
  auto modules = facebook::jsi::Object::createFromHostObject(
      runtime,
      std::make_shared<ExpoQnxModulesHostObject>());
  expo.setProperty(runtime, "modules", std::move(modules));

  // Expo Modules Core's native bootstrap first checks globalThis.expo. By
  // installing this before the JS bundle runs, QNX follows the native-startup
  // path and does not need an Android-style ExpoModulesCore bridge module.
  runtime.global().setProperty(runtime, "expo", std::move(expo));
}

} // namespace rnqnx::expo
