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

class ExpoQnxModuleRegistry final {
public:
  static ExpoQnxModuleRegistry& instance();

  void registerModule(std::string name, ModuleFactory factory);
  bool hasModule(const std::string& name) const;
  facebook::jsi::Object createModule(
      facebook::jsi::Runtime& runtime,
      const std::string& name) const;
  std::vector<std::string> names() const;

private:
  ExpoQnxModuleRegistry() = default;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, ModuleFactory> factories_;
};

} // namespace rnqnx::expo
