#include "ExpoQnxInstaller.hpp"
#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <QGuiApplication>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void evaluate(facebook::jsi::Runtime& runtime, const std::string& source) {
  try {
    runtime.evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>(source), "constants-test.js");
  } catch (const facebook::jsi::JSError& error) {
    throw std::runtime_error(error.what());
  }
}
void check(facebook::jsi::Runtime& runtime, const std::string& condition) {
  evaluate(runtime, "if (!(" + condition + ")) throw new Error(" +
      "'Constants assertion: ' + " + "JSON.stringify(" + condition + "));");
}
}
int main(int argc, char** argv) {
  try {
    QGuiApplication application(argc, argv);
    auto runtime = facebook::hermes::makeHermesRuntime();
    rnqnx::expo::installExpoModulesQnx(*runtime);
#if RNQNX_ENABLE_EXPO_CONSTANTS
    evaluate(*runtime, "globalThis.__DEV__ = true;");
    check(*runtime, "expo.modules.ExponentConstants.executionEnvironment === 'bare'");
    check(*runtime, "expo.modules.ExponentConstants.appOwnership === null");
    check(*runtime, "expo.modules.ExponentConstants.debugMode === true");
    check(*runtime, "expo.modules.ExponentConstants.isHeadless === false");
    check(*runtime, "expo.modules.ExponentConstants.sessionId.length === 36");
    check(*runtime, "Array.isArray(expo.modules.ExponentConstants.systemFonts)");
    check(*runtime, "expo.modules.ExponentConstants.deviceName === undefined");
    check(*runtime, "expo.modules.ExponentConstants.getWebViewUserAgentAsync === undefined");
    check(*runtime, "expo.modules.ExponentConstants === expo.modules.ExponentConstants");
#if RNQNX_TEST_CONFIGURED
    check(*runtime, "expo.modules.ExponentConstants.manifest.name === 'Café Consumer'");
    check(*runtime, "expo.modules.ExponentConstants.linkingUri === 'consumer://'");
#else
    check(*runtime, "expo.modules.ExponentConstants.manifest === null");
    check(*runtime, "expo.modules.ExponentConstants.linkingUri === undefined");
#endif
    if (argc > 1) {
      std::ifstream file(argv[1]);
      if (!file.good()) throw std::runtime_error("Missing public SDK fixture");
      std::ostringstream source; source << file.rdbuf(); evaluate(*runtime, source.str());
#if RNQNX_TEST_CONFIGURED
      check(*runtime, "__expoConstantsPublic.expoConfig.scheme === 'consumer'");
      check(*runtime, "__expoConstantsPublic.expoConfig.extra.router.origin === false");
#else
      check(*runtime, "__expoConstantsPublic.expoConfig === null");
#endif
      check(*runtime, "__expoConstantsPublic.executionEnvironment === 'bare'");
      check(*runtime, "__expoConstantsPublic.expoVersion === null");
    }
    auto second = facebook::hermes::makeHermesRuntime();
    rnqnx::expo::installExpoModulesQnx(*second);
#if RNQNX_TEST_CONFIGURED
    evaluate(*runtime, "expo.modules.ExponentConstants.manifest.name = 'changed';");
    check(*second, "expo.modules.ExponentConstants.manifest.name === 'Café Consumer'");
#else
    check(*second, "expo.modules.ExponentConstants.manifest === null");
#endif
    check(*second, "expo.modules.ExponentConstants.debugMode === false");
    runtime.reset();
#if RNQNX_TEST_CONFIGURED
    check(*second, "expo.modules.ExponentConstants.linkingUri === 'consumer://'");
#endif
    bool rejected = false;
    std::thread worker([&] {
      auto isolated = facebook::hermes::makeHermesRuntime();
      rnqnx::expo::installExpoModulesQnx(*isolated);
      try { evaluate(*isolated, "expo.modules.ExponentConstants"); }
      catch (const std::exception&) { rejected = true; }
    });
    worker.join();
    if (!rejected) throw std::runtime_error("Worker access was not rejected");
    std::puts("EXPO_CONSTANTS_HOST_ON_OK public config, session, official SDK, per-runtime isolation and thread rejection");
#else
    check(*runtime, "expo.modules.ExponentConstants === undefined");
    std::puts("EXPO_CONSTANTS_HOST_OFF_OK");
#endif
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "EXPO_CONSTANTS_FAIL %s\n", error.what()); return 1;
  }
}
