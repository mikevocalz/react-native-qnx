#include "ExpoQnxModulesHostObject.hpp"

#include <stdexcept>

namespace rnqnx::expo {

facebook::jsi::Value ExpoQnxModulesHostObject::get(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::PropNameID& name) {
  const auto moduleName = name.utf8(runtime);

  if (auto found = cache_.find(moduleName); found != cache_.end()) {
    return facebook::jsi::Value(runtime, *found->second);
  }

  auto& registry = ExpoQnxModuleRegistry::instance();
  if (!registry.hasModule(moduleName)) {
    return facebook::jsi::Value::undefined();
  }

  auto module = std::make_unique<facebook::jsi::Object>(
      registry.createModule(runtime, moduleName));
  auto value = facebook::jsi::Value(runtime, *module);
  cache_.insert_or_assign(moduleName, std::move(module));
  return value;
}

void ExpoQnxModulesHostObject::set(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::PropNameID& name,
    const facebook::jsi::Value&) {
  throw facebook::jsi::JSError(
      runtime,
      "Cannot override Expo QNX module '" + name.utf8(runtime) + "'");
}

std::vector<facebook::jsi::PropNameID>
ExpoQnxModulesHostObject::getPropertyNames(facebook::jsi::Runtime& runtime) {
  const auto names = ExpoQnxModuleRegistry::instance().names();
  std::vector<facebook::jsi::PropNameID> result;
  result.reserve(names.size());
  for (const auto& name : names) {
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, name));
  }
  return result;
}

} // namespace rnqnx::expo
