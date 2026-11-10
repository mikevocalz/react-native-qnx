#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <cstdio>
#include <memory>
#include <stdexcept>
struct Fixture {
  QQmlEngine engine;
  rnqnx::views::QtQuickItemFactory factory{engine};
  QQuickItem root;
  rnqnx::fabric::QtQuickMountingLayer layer{factory, root, 1};
  rnqnx::fabric::ShadowViewMutationAdapter adapter{layer};
  rnqnx::runtime::QtReactHost host{adapter};
  void check(const std::string& expression) {
    if (!host.evaluate("if (!(" + expression + ")) throw new Error('autolink assertion failed');", "autolink-contract.js"))
      throw std::runtime_error(host.lastError().toStdString());
  }
};
int main(int argc, char** argv) {
  try {
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName("Autolink native contract");
    auto first = std::make_unique<Fixture>();
#if RNQNX_ENABLE_EXPO_MODULES
    first->check("expo.modules.ExpoAutolinkContract.read().name === 'Autolink native contract'");
    first->check("expo.modules.ExpoAutolinkContract.read().calls === 2");
    first->check("expo.modules.ExpoApplication === undefined");
    auto second = std::make_unique<Fixture>();
    second->check("expo.modules.ExpoAutolinkContract.read().calls === 1");
    first.reset();
    second->check("expo.modules.ExpoAutolinkContract.read().calls === 2");
    second.reset();
    Fixture recreated;
    recreated.check("expo.modules.ExpoAutolinkContract.read().calls === 1");
#else
    first->check("typeof expo === 'undefined'");
#endif
    std::puts("EXPO_AUTOLINK_HOST_OK QtReactHost generated provider invocation and disposal");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
