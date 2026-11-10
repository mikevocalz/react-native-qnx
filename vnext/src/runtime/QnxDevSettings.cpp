#include "react-native-qnx/runtime/QnxDevSettings.h"

#include <utility>
#include <QUrl>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

QnxDevSettings::QnxDevSettings(std::shared_ptr<react::CallInvoker> invoker,
    Reload reloadCallback, Report report)
    : react::TurboModule(kModuleName, std::move(invoker)),
      reload_(std::move(reloadCallback)), report_(std::move(report)) {
  methodMap_["reload"] = {0, &QnxDevSettings::reload};
  methodMap_["reloadWithReason"] = {1, &QnxDevSettings::reload};
  methodMap_["onFastRefresh"] = {0, &QnxDevSettings::refreshed};
  for (const char* name : {"setHotLoadingEnabled", "setProfilingEnabled",
       "toggleElementInspector", "addMenuItem", "openDebugger", "addListener",
       "removeListeners", "setIsShakeToShowDevMenuEnabled"}) {
    methodMap_[name] = {1, &QnxDevSettings::unsupported};
  }
}

jsi::Value QnxDevSettings::reload(jsi::Runtime& rt, react::TurboModule& module,
    const jsi::Value* args, size_t count) {
  if (count > 1 || (count == 1 && !args[0].isString())) {
    throw jsi::JSError(rt, "DevSettings.reload requires an optional string reason");
  }
  static_cast<QnxDevSettings&>(module).reload_(count == 0 ? "manual reload"
      : args[0].asString(rt).utf8(rt));
  return jsi::Value::undefined();
}

jsi::Value QnxDevSettings::refreshed(jsi::Runtime& rt, react::TurboModule& module,
    const jsi::Value*, size_t count) {
  if (count != 0) throw jsi::JSError(rt, "onFastRefresh takes no arguments");
  static_cast<QnxDevSettings&>(module).report_("refreshed", "React Refresh completed");
  return jsi::Value::undefined();
}

jsi::Value QnxDevSettings::unsupported(jsi::Runtime& rt, react::TurboModule&,
    const jsi::Value*, size_t) {
  throw jsi::JSError(rt,
      "This host development session supports reload and React Refresh only; "
      "inspector, profiling, developer menu and runtime hot-toggle are unavailable");
}

void installDevGlobals(jsi::Runtime& rt, const std::string& bundleUrl,
    std::uint64_t generation, QnxDevSettings::Report report) {
  auto config = jsi::Object(rt);
  config.setProperty(rt, "bundleUrl", jsi::String::createFromUtf8(rt, bundleUrl));
  QUrl hotUrl(QString::fromStdString(bundleUrl));
  hotUrl.setScheme(hotUrl.scheme() == "https" ? "wss" : "ws");
  hotUrl.setPath(QStringLiteral("/hot"));
  hotUrl.setQuery(QString());
  config.setProperty(rt, "hotUrl", jsi::String::createFromUtf8(rt,
      hotUrl.toString(QUrl::FullyEncoded).toStdString()));
  config.setProperty(rt, "generation", static_cast<double>(generation));
  rt.global().getPropertyAsObject(rt, "Object").getPropertyAsFunction(rt, "freeze")
      .call(rt, config);
  rt.global().setProperty(rt, "__RNQNX_DEV_SESSION__", std::move(config));
  rt.global().setProperty(rt, "__RNQNX_DEV_REPORT__",
      jsi::Function::createFromHostFunction(rt,
          jsi::PropNameID::forAscii(rt, "__RNQNX_DEV_REPORT__"), 2,
          [report](jsi::Runtime& runtime, const jsi::Value&,
              const jsi::Value* args, size_t count) {
            if (count != 2 || !args[0].isString() || !args[1].isString()) {
              throw jsi::JSError(runtime, "Development status requires kind and message");
            }
            report(args[0].asString(runtime).utf8(runtime),
                   args[1].asString(runtime).utf8(runtime));
            return jsi::Value::undefined();
          }));
  rt.global().setProperty(rt, "globalEvalWithSourceUrl",
      jsi::Function::createFromHostFunction(rt,
          jsi::PropNameID::forAscii(rt, "globalEvalWithSourceUrl"), 2,
          [report](jsi::Runtime& runtime, const jsi::Value&,
              const jsi::Value* args, size_t count) -> jsi::Value {
            if (count != 2 || !args[0].isString() || !args[1].isString()) {
              throw jsi::JSError(runtime, "Metro injection requires source and sourceURL");
            }
            const auto source = args[0].asString(runtime).utf8(runtime);
            const auto sourceUrl = args[1].asString(runtime).utf8(runtime);
            if (source.size() > 16 * 1024 * 1024 || sourceUrl.size() > 8192) {
              throw jsi::JSError(runtime, "Metro module exceeds development injection limit");
            }
            // Called by Metro inside the existing RN scheduler task. Do not
            // reschedule individual modules or change SourceCode.scriptURL.
            try {
              return runtime.evaluateJavaScript(
                  std::make_unique<jsi::StringBuffer>(source), sourceUrl);
            } catch (const jsi::JSError& error) {
              report("error", error.getMessage() + "\n" + error.getStack());
              throw;
            }
          }));
}
} // namespace rnqnx::runtime
