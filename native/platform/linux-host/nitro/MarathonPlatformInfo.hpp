#pragma once

#include <NitroModules/HybridClass.hpp>
#include <NitroModules/HybridObject.hpp>

#include <memory>
#include <string>

namespace rnqnx::nitro {

// MarathonOS Linux/Wayland native implementation of the shared typed spec.
// The generated HybridClass descriptor lives in generated/*.binding.cpp.
class MarathonPlatformInfo final : public margelo::nitro::HybridObject {
public:
  MarathonPlatformInfo() = default;

  struct Class {
    using Base = margelo::nitro::HybridObject::Class;
    static constexpr const char* name = "MarathonPlatformInfo";
    static const std::shared_ptr<const margelo::nitro::HybridClass>& get();
  };

  static std::shared_ptr<MarathonPlatformInfo> create();

  std::string getPlatform();
  std::string getArchitecture();
  double add(double left, double right);

protected:
  const std::shared_ptr<const margelo::nitro::HybridClass>& getHybridClass() override {
    return Class::get();
  }
};

} // namespace rnqnx::nitro
