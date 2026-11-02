#include "MarathonPlatformInfo.hpp"

namespace rnqnx::nitro {

std::shared_ptr<MarathonPlatformInfo> MarathonPlatformInfo::create() {
  return std::make_shared<MarathonPlatformInfo>();
}

std::string MarathonPlatformInfo::getPlatform() {
  return "marathon";
}

std::string MarathonPlatformInfo::getArchitecture() {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__x86_64__)
  return "x86_64";
#else
  return "unknown";
#endif
}

double MarathonPlatformInfo::add(double left, double right) {
  return left + right;
}

} // namespace rnqnx::nitro
