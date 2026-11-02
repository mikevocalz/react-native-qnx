#include "QnxPlatformInfo.hpp"

#include <NitroModules/HybridClass.hpp>

namespace rnqnx::nitro {

std::shared_ptr<QnxPlatformInfo> QnxPlatformInfo::create() {
  return std::make_shared<QnxPlatformInfo>();
}

const std::shared_ptr<const margelo::nitro::HybridClass>&
QnxPlatformInfo::Class::get() {
  static const auto cls = margelo::nitro::HybridClass::create<Class>(
      [](margelo::nitro::HybridClass& descriptor) {
        descriptor.registerHybridConstructor(&QnxPlatformInfo::create);
        descriptor.registerHybridProperty("platform", &QnxPlatformInfo::getPlatform);
        descriptor.registerHybridProperty("architecture", &QnxPlatformInfo::getArchitecture);
        descriptor.registerHybridMethod("add", &QnxPlatformInfo::add);
      });
  return cls;
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
