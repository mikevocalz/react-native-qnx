#include "QnxPlatformInfo.hpp"

#include "Prototype.hpp"

namespace rnqnx::nitro {

QnxPlatformInfo::QnxPlatformInfo() : HybridObject(TAG) {}

std::string QnxPlatformInfo::getPlatform() const {
  return "qnx";
}

std::string QnxPlatformInfo::getArchitecture() const {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__x86_64__)
  return "x86_64";
#else
  return "unknown";
#endif
}

double QnxPlatformInfo::add(double left, double right) const {
  return left + right;
}

void QnxPlatformInfo::loadHybridMethods() {
  HybridObject::loadHybridMethods();
  registerHybrids(this, [](margelo::nitro::Prototype& prototype) {
    prototype.registerHybridGetter("platform", &QnxPlatformInfo::getPlatform);
    prototype.registerHybridGetter("architecture", &QnxPlatformInfo::getArchitecture);
    prototype.registerHybridMethod("add", &QnxPlatformInfo::add);
  });
}

} // namespace rnqnx::nitro
