#include "QtNitroDispatcher.hpp"
#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"
#include RNQNX_NATIVE_PACKAGES_HEADER

#include <NitroModules/InstallNitro.hpp>
#include <NitroModules/HybridClassRegistry.hpp>
#ifdef NATIVE_PACKAGE_MARATHON
#include "MarathonNitroRuntime.hpp"
#else
#include "QnxNitroRuntime.hpp"
#endif
#include <hermes/hermes.h>
#include <hermes/Public/RuntimeConfig.h>
#include <QCoreApplication>

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

int main(int argc, char** argv) {
  try {
    if (argc != 2) throw std::runtime_error("Pass the installed-package JavaScript bundle");
    QCoreApplication app(argc, argv);
    auto runtime = facebook::hermes::makeHermesRuntime(
        ::hermes::vm::RuntimeConfig::Builder().withMicrotaskQueue(true).build());
    auto& rt = *runtime;
    const auto owner = std::this_thread::get_id();
    unsigned entryDepth = 0;
    unsigned completions = 0;
    auto enter = [&] {
      if (std::this_thread::get_id() != owner) throw std::runtime_error("JavaScript entered off the Qt owner");
      ++entryDepth;
    };
    auto leave = [&] {
      if (--entryDepth == 0) rt.drainMicrotasks();
    };
    std::string failure;
    auto queue = std::make_shared<rnqnx::runtime::QtRuntimeTaskQueue>(
        [&](auto task) {
          try { enter(); task(); leave(); ++completions; }
          catch (const std::exception& error) { failure = error.what(); }
        }, enter, leave);
    auto dispatcher = std::make_shared<rnqnx::nitro::QtNitroDispatcher>(queue);
#ifdef NATIVE_PACKAGE_MARATHON
    rnqnx::nitro::installMarathonNitroRuntime(rt, dispatcher);
    constexpr auto ownClass = "MarathonPlatformInfo";
    constexpr auto otherClass = "QnxPlatformInfo";
    constexpr auto platform = "marathon";
#else
    rnqnx::nitro::installQnxNitroRuntime(rt, dispatcher);
    constexpr auto ownClass = "QnxPlatformInfo";
    constexpr auto otherClass = "MarathonPlatformInfo";
    constexpr auto platform = "qnx";
#endif
    if (!margelo::nitro::HybridClassRegistry::hasHybridClass(ownClass) ||
        margelo::nitro::HybridClassRegistry::hasHybridClass(otherClass))
      throw std::runtime_error("Native platform class registration crossed platform boundaries");
    const std::string identity = std::string("(new (NitroModulesProxy.getHybridClass('") + ownClass + "'))()).platform";
    const auto nativePlatform = rt.evaluateJavaScript(
        std::make_unique<facebook::jsi::StringBuffer>(identity), "platform-identity");
    if (nativePlatform.asString(rt).utf8(rt) != platform)
      throw std::runtime_error("Native platform identity did not match the selected adapter");
    rnqnx::nativepackages::registerPackages();
    rnqnx::nativepackages::registerPackages(); // Package installation is idempotent.
    std::ifstream file(argv[1], std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot open consumer bundle");
    const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    rt.evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>(source), argv[1]);
    rt.drainMicrotasks();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (failure.empty() && std::chrono::steady_clock::now() < deadline) {
      QCoreApplication::processEvents();
      const auto done = rt.global().getProperty(rt, "nativePackageDone");
      if (done.isBool() && done.getBool()) break;
      std::this_thread::yield();
    }
    const auto result = rt.global().getProperty(rt, "nativePackageResult");
    if (failure.empty() && result.isString()) failure = result.asString(rt).utf8(rt);
    const auto done = rt.global().getProperty(rt, "nativePackageDone");
    const bool completed = done.isBool() && done.getBool();
    queue->close();
    if (!failure.empty()) throw std::runtime_error(failure);
    if (!completed || completions == 0) throw std::runtime_error("Native package promises did not complete through Qt");
    std::cout << "NATIVE_PACKAGE_QT_HERMES_OK completions=" << completions
              << " owner-thread=1 dispatcher=real installed-registration=1 platform=" << platform << '\n';
  } catch (const std::exception& error) {
    std::cerr << "NATIVE_PACKAGE_QT_HERMES_FAILED " << error.what() << '\n';
    return 1;
  }
}
