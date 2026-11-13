// ExponentConstants for QNX: the constants expo-constants 58 reads from its
// native module (src/Constants.ts), taken from the running system and from
// the app config the consumer build embedded.
#include "ExpoQnxInstaller.hpp"
#include "ExpoQnxModuleRegistry.hpp"
#include "ExpoQnxUuid.hpp"

#include <jsi/jsi.h>

#include <sys/utsname.h>

#include <string>
#include <utility>

namespace rnqnx::expo::constants {
namespace {

namespace jsi = facebook::jsi;

struct SystemInfo {
  std::string nodeName;
  std::string release;
  std::string machine;
  std::string sysName;
};

SystemInfo systemInfo() {
  struct utsname name {};
  if (uname(&name) != 0) return {};
  return {name.nodename, name.release, name.machine, name.sysname};
}

jsi::Object createModule(jsi::Runtime& runtime) {
  const SystemInfo system = systemInfo();
  jsi::Object module(runtime);
  module.setProperty(runtime, "name", "ExponentConstants");
  // A standalone app, not Expo Go or a store client.
  module.setProperty(runtime, "appOwnership", jsi::Value::null());
  module.setProperty(runtime, "executionEnvironment", "bare");
  module.setProperty(runtime, "debugMode", debugBuild());
  module.setProperty(runtime, "isHeadless", false);
  module.setProperty(runtime, "deviceName", system.nodeName);
  module.setProperty(runtime, "deviceYearClass", jsi::Value::null());
  module.setProperty(runtime, "statusBarHeight", 0);
  module.setProperty(runtime, "systemFonts", jsi::Array(runtime, 0));
  module.setProperty(runtime, "sessionId", uuidV4());
  module.setProperty(runtime, "linkingUri", "");
  module.setProperty(runtime, "experienceUrl", "");
  module.setProperty(runtime, "expoVersion", jsi::Value::null());
  module.setProperty(runtime, "expoRuntimeVersion", jsi::Value::null());

  // The app config as a JSON string, the form Android uses; Constants.ts
  // parses a string manifest.
  const std::string& manifest = appManifestJson();
  if (manifest.empty()) {
    module.setProperty(runtime, "manifest", jsi::Value::null());
  } else {
    module.setProperty(runtime, "manifest", jsi::String::createFromUtf8(runtime, manifest));
  }

  jsi::Object qnx(runtime);
  qnx.setProperty(runtime, "systemName", system.sysName);
  qnx.setProperty(runtime, "systemVersion", system.release);
  qnx.setProperty(runtime, "machine", system.machine);
  jsi::Object platform(runtime);
  platform.setProperty(runtime, "qnx", std::move(qnx));
  module.setProperty(runtime, "platform", std::move(platform));

  module.setProperty(runtime, "getWebViewUserAgentAsync",
      jsi::Function::createFromHostFunction(runtime,
          jsi::PropNameID::forAscii(runtime, "getWebViewUserAgentAsync"), 0,
          [](jsi::Runtime& rt, const jsi::Value&, const jsi::Value*, size_t) {
            // There is no WebView on QNX: resolve to null, as the type allows.
            auto promise = rt.global().getPropertyAsFunction(rt, "Promise");
            return promise.getPropertyAsFunction(rt, "resolve")
                .callWithThis(rt, promise, jsi::Value::null());
          }));
  return module;
}

} // namespace

void registerModule() {
  ExpoQnxModuleRegistry::instance().registerModule("ExponentConstants", createModule);
}

} // namespace rnqnx::expo::constants
