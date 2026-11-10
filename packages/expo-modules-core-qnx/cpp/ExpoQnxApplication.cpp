#include "ExpoQnxApplication.hpp"
#include "ExpoQnxModuleRegistry.hpp"
namespace rnqnx::expo {
namespace {
void setOptionalString(facebook::jsi::Runtime& runtime,
                       facebook::jsi::Object& module,
                       const char* name,
                       const std::optional<std::string>& value) {
  if (value) {
    module.setProperty(runtime, name, facebook::jsi::String::createFromUtf8(runtime, *value));
  } else {
    module.setProperty(runtime, name, facebook::jsi::Value::null());
  }
}
} // namespace
void registerApplicationModule() {
  ExpoQnxModuleRegistry::instance().registerModule(
      "ExpoApplication", [](facebook::jsi::Runtime& runtime) {
        // Acquire a snapshot on the existing runtime/application owner. The
        // Expo host object caches it independently for each Hermes runtime.
        const auto metadata = readApplicationMetadata();
        facebook::jsi::Object module(runtime);
        setOptionalString(runtime, module, "applicationId", metadata.applicationId);
        setOptionalString(runtime, module, "applicationName", metadata.applicationName);
        setOptionalString(runtime, module, "nativeApplicationVersion", metadata.nativeApplicationVersion);
        // Qt has no separate build number; do not invent one.
        module.setProperty(runtime, "nativeBuildVersion", facebook::jsi::Value::null());
        // Platform-specific APIs remain absent; Expo's public API rejects them.
        return module;
      });
}
} // namespace rnqnx::expo
