#pragma once

#include <NitroModules/HybridClass.hpp>
#include <NitroModules/HybridObject.hpp>
#include <memory>
#include <string>

namespace rnqnx::nitro {

// Nitro v1 models HybridObjects as shared HybridClass descriptors with class
// constructors/prototypes, not v0's loadHybridMethods/Prototype registry.
class QnxPlatformInfo final : public margelo::nitro::HybridObject {
public:
  QnxPlatformInfo() = default;

  struct Class {
    using Base = margelo::nitro::HybridObject::Class;
    static constexpr const char* name = "QnxPlatformInfo";
    static const std::shared_ptr<const margelo::nitro::HybridClass>& get();
  };

  static std::shared_ptr<QnxPlatformInfo> create();

  std::string getPlatform();
  std::string getArchitecture();
  double add(double left, double right);

protected:
  const std::shared_ptr<const margelo::nitro::HybridClass>& getHybridClass() override {
    return Class::get();
  }
};

} // namespace rnqnx::nitro
