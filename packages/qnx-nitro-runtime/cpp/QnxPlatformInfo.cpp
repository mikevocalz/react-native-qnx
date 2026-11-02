#include "QnxPlatformInfo.hpp"


namespace rnqnx::nitro {

std::shared_ptr<QnxPlatformInfo> QnxPlatformInfo::create() {
  return std::make_shared<QnxPlatformInfo>();
}

std::string QnxPlatformInfo::getPlatform() {
  return "qnx";
}

std::string QnxPlatformInfo::getArchitecture() {
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__x86_64__)
  return "x86_64";
#else
  return "unknown";
#endif
}

double QnxPlatformInfo::add(double left, double right) {
  return left + right;
}

} // namespace rnqnx::nitro
