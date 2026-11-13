#include "ExpoQnxModuleRegistry.hpp"

#include <stdexcept>
#include <utility>

namespace rnqnx::expo {

ExpoQnxModuleRegistry& ExpoQnxModuleRegistry::instance() {
  static ExpoQnxModuleRegistry registry;
  return registry;
}

void ExpoQnxModuleRegistry::registerModule(
    std::string name,
    ModuleFactory factory) {
  if (name.empty()) {
    throw std::invalid_argument("Expo QNX module name cannot be empty");
  }
  if (!factory) {
    throw std::invalid_argument("Expo QNX module factory cannot be empty");
  }

  std::lock_guard lock(mutex_);
  factories_.insert_or_assign(std::move(name), std::move(factory));
}

bool ExpoQnxModuleRegistry::hasModule(const std::string& name) const {
  std::lock_guard lock(mutex_);
  return factories_.contains(name);
}

facebook::jsi::Object ExpoQnxModuleRegistry::createModule(
    facebook::jsi::Runtime& runtime,
    const std::string& name) const {
  ModuleFactory factory;
  {
    std::lock_guard lock(mutex_);
    auto found = factories_.find(name);
    if (found == factories_.end()) {
      throw std::out_of_range("Unknown Expo QNX module: " + name);
    }
    factory = found->second;
  }
  return factory(runtime);
}

namespace {
// A NUL cannot appear in either name, so it keeps ("A", "B") apart from ("A/B", "").
std::string viewKey(const std::string& moduleName, const std::string& viewName) {
  return moduleName + '\0' + viewName;
}
} // namespace

void ExpoQnxModuleRegistry::registerViewConfig(
    std::string moduleName,
    std::string viewName,
    ExpoViewConfig config) {
  if (moduleName.empty()) {
    throw std::invalid_argument("Expo QNX view module name cannot be empty");
  }
  std::lock_guard lock(mutex_);
  viewConfigs_.insert_or_assign(viewKey(moduleName, viewName), std::move(config));
}

bool ExpoQnxModuleRegistry::findViewConfig(
    const std::string& moduleName,
    const std::string& viewName,
    ExpoViewConfig& out) const {
  std::lock_guard lock(mutex_);
  const auto found = viewConfigs_.find(viewKey(moduleName, viewName));
  if (found == viewConfigs_.end()) {
    return false;
  }
  out = found->second;
  return true;
}

std::vector<std::string> ExpoQnxModuleRegistry::names() const {
  std::lock_guard lock(mutex_);
  std::vector<std::string> result;
  result.reserve(factories_.size());
  for (const auto& [name, _] : factories_) {
    result.push_back(name);
  }
  return result;
}

} // namespace rnqnx::expo
