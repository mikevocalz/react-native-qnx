#include "MarathonNitroProof.hpp"
#include "MarathonPlatformInfo.hpp"

#include <stdexcept>

namespace rnqnx::nitro {

const char* const kMarathonNitroProofSource = R"JS(
(function () {
  var proxy = globalThis.NitroModulesProxy;
  if (proxy == null) {
    throw new Error('NitroModulesProxy is not installed');
  }
  var Info = proxy.getHybridClass('MarathonPlatformInfo');
  var info = new Info();
  globalThis.__rnqnxMarathonNitro = {
    platform: info.platform,
    architecture: info.architecture,
    sum: info.add(2, 3),
  };
})();
)JS";

std::string verifyMarathonNitroProof(facebook::jsi::Runtime& runtime) {
  namespace jsi = facebook::jsi;
  const jsi::Value stored =
      runtime.global().getProperty(runtime, "__rnqnxMarathonNitro");
  if (!stored.isObject()) {
    throw std::runtime_error("the Nitro proof script stored no result");
  }
  const jsi::Object result = stored.asObject(runtime);
  const auto text = [&](const char* name) {
    const jsi::Value value = result.getProperty(runtime, name);
    if (!value.isString()) {
      throw std::runtime_error(std::string("proof field is not a string: ") + name);
    }
    return value.asString(runtime).utf8(runtime);
  };
  const jsi::Value sumValue = result.getProperty(runtime, "sum");
  if (!sumValue.isNumber()) {
    throw std::runtime_error("proof field is not a number: sum");
  }

  const std::string platform = text("platform");
  const std::string architecture = text("architecture");
  const double sum = sumValue.asNumber();
  if (platform != "marathon") {
    throw std::runtime_error("MarathonPlatformInfo.platform was " + platform);
  }
  if (sum != 5) {
    throw std::runtime_error("MarathonPlatformInfo.add(2, 3) did not return 5");
  }
  // The native object computes the expected architecture, so a JS-side stub
  // cannot satisfy this check.
  const auto native = MarathonPlatformInfo::create();
  if (architecture != native->getArchitecture()) {
    throw std::runtime_error("MarathonPlatformInfo.architecture was " + architecture);
  }
  return "platform=" + platform + " architecture=" + architecture + " add=5";
}

} // namespace rnqnx::nitro
