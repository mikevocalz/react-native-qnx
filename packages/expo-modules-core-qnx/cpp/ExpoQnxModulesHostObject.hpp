#pragma once

#include "ExpoQnxModuleRegistry.hpp"

#include <jsi/jsi.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace rnqnx::expo {

class ExpoQnxModulesHostObject final : public facebook::jsi::HostObject {
public:
  facebook::jsi::Value get(
      facebook::jsi::Runtime& runtime,
      const facebook::jsi::PropNameID& name) override;

  void set(
      facebook::jsi::Runtime& runtime,
      const facebook::jsi::PropNameID& name,
      const facebook::jsi::Value& value) override;

  std::vector<facebook::jsi::PropNameID> getPropertyNames(
      facebook::jsi::Runtime& runtime) override;

private:
  std::unordered_map<std::string, std::unique_ptr<facebook::jsi::Object>> cache_;
};

} // namespace rnqnx::expo
