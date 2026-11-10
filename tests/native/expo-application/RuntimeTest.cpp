#include "ExpoQnxInstaller.hpp"
#if RNQNX_ENABLE_EXPO_APPLICATION
#include "ExpoQnxApplication.hpp"
#endif
#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
[[maybe_unused]] void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void assertJS(facebook::jsi::Runtime& runtime, const std::string& expression) {
  runtime.evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>(
      "if (!(" + expression + ")) throw new Error('ExpoApplication assertion failed: ' + " +
      "JSON.stringify(" + expression + "))"), "expo-application-test.js");
}
} // namespace

int main(int argc, char** argv) {
  try {
#if RNQNX_ENABLE_EXPO_APPLICATION
    bool missingAppRejected = false;
    try { (void)rnqnx::expo::readApplicationMetadata(); }
    catch (const std::runtime_error&) { missingAppRejected = true; }
    require(missingAppRejected, "Missing Qt application must reject metadata access");
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("com.example.consumer"));
    QGuiApplication::setDesktopFileName(QStringLiteral("com.example.consumer"));
    QGuiApplication::setApplicationDisplayName(QString::fromUtf8("Café Consumer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.4.1"));
    auto runtime = facebook::hermes::makeHermesRuntime();
    rnqnx::expo::installExpoModulesQnx(*runtime);
#if RNQNX_ENABLE_EXPO_APPLICATION
    assertJS(*runtime, "expo.modules.ExpoApplication.applicationId === 'com.example.consumer'");
    assertJS(*runtime, "expo.modules.ExpoApplication.applicationName === 'Café Consumer'");
    assertJS(*runtime, "expo.modules.ExpoApplication.nativeApplicationVersion === '2.4.1'");
    assertJS(*runtime, "expo.modules.ExpoApplication.nativeBuildVersion === null");
    assertJS(*runtime, "expo.modules.ExpoApplication.getInstallationTimeAsync === undefined");
    assertJS(*runtime, "expo.modules.ExpoCamera === undefined");
    assertJS(*runtime, "expo.modules.ExpoApplication === expo.modules.ExpoApplication");
    if (argc > 1) {
      std::ifstream input(argv[1]);
      require(input.good(), "Cannot read official ExpoApplication fixture");
      std::ostringstream source;
      source << input.rdbuf();
      runtime->evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>(source.str()), "official-expo-application.js");
      assertJS(*runtime, "__expoApplicationPublic.applicationId === 'com.example.consumer'");
      assertJS(*runtime, "__expoApplicationPublic.applicationName === 'Café Consumer'");
      assertJS(*runtime, "__expoApplicationPublic.nativeApplicationVersion === '2.4.1'");
      assertJS(*runtime, "__expoApplicationPublic.nativeBuildVersion === null");
      assertJS(*runtime, "(() => { try { __expoApplicationPublic.getAndroidId(); } catch (e) { return e.code === 'ERR_UNAVAILABLE'; } return false; })()");
      std::puts("EXPO_APPLICATION_PUBLIC_SDK58_OK official Application.ts and native acquisition against real provider");
    }
    QCoreApplication::setApplicationVersion(QString());
    QGuiApplication::setDesktopFileName(QString());
    // Existing runtime retains its acquired constants; a new runtime has a new
    // snapshot and must never acquire objects owned by the previous Hermes VM.
    auto second = facebook::hermes::makeHermesRuntime();
    rnqnx::expo::installExpoModulesQnx(*second);
    assertJS(*runtime, "expo.modules.ExpoApplication.nativeApplicationVersion === '2.4.1'");
    assertJS(*second, "expo.modules.ExpoApplication.nativeApplicationVersion === null");
    assertJS(*second, "expo.modules.ExpoApplication.applicationId === null");
    runtime.reset();
    assertJS(*second, "expo.modules.ExpoApplication.applicationName === 'Café Consumer'");
    bool rejected = false;
    std::thread worker([&] {
      try { (void)rnqnx::expo::readApplicationMetadata(); }
      catch (const std::runtime_error&) { rejected = true; }
    });
    worker.join();
    require(rejected, "Qt owner-thread rule must reject worker access");
    std::puts("EXPO_APPLICATION_HOST_ON_OK actual Qt metadata, Hermes registration, lifetime, nulls and thread rejection");
#else
    assertJS(*runtime, "expo.modules.ExpoApplication === undefined");
    std::puts("EXPO_APPLICATION_HOST_OFF_OK no module registered");
#endif
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "EXPO_APPLICATION_HOST_FAIL %s\n", error.what());
    return 1;
  }
}
