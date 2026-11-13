#pragma once

#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace rnqnx::expo {

using ModuleFactory =
    std::function<facebook::jsi::Object(facebook::jsi::Runtime&)>;

/**
 * What an Expo view declares to React Native: the props it accepts and its
 * direct events. `globalThis.expo.getViewConfig(module, view)` returns this as
 * { validAttributes, directEventTypes }, the shape expo-modules-core's
 * requireNativeView builds its host component from.
 */
struct ExpoViewConfig {
  /** Prop names, for example "contentFit". */
  std::vector<std::string> props;
  /** Event handler props, for example "onLoad". Each is a direct event named
   * `top` + the name without `on` ("topLoad"). */
  std::vector<std::string> events;
};

class ExpoQnxModuleRegistry final {
public:
  static ExpoQnxModuleRegistry& instance();

  void registerModule(std::string name, ModuleFactory factory);
  bool hasModule(const std::string& name) const;
  facebook::jsi::Object createModule(
      facebook::jsi::Runtime& runtime,
      const std::string& name) const;
  std::vector<std::string> names() const;

  /**
   * Declares a view's props and events. \a viewName is empty for a module's
   * default view (`requireNativeView('ExpoImage')`). Re-registering replaces.
   */
  void registerViewConfig(std::string moduleName, std::string viewName, ExpoViewConfig config);
  bool findViewConfig(const std::string& moduleName, const std::string& viewName, ExpoViewConfig& out) const;

private:
  ExpoQnxModuleRegistry() = default;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, ModuleFactory> factories_;
  std::unordered_map<std::string, ExpoViewConfig> viewConfigs_;
};

} // namespace rnqnx::expo
