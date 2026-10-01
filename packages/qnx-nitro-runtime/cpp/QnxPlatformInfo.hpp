#pragma once

#include "HybridObject.hpp"
#include <string>

namespace rnqnx::nitro {

class QnxPlatformInfo final : public margelo::nitro::HybridObject {
public:
  QnxPlatformInfo();

  std::string getPlatform() const;
  std::string getArchitecture() const;
  double add(double left, double right) const;

protected:
  void loadHybridMethods() override;

private:
  static constexpr auto TAG = "QnxPlatformInfo";
};

} // namespace rnqnx::nitro
