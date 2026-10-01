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
