#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>

#include <iostream>

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QQmlEngine engine;
  QQuickItem root;

  rnqnx::views::QtQuickItemFactory factory(engine);
  rnqnx::fabric::QtQuickMountingLayer layer(factory, root, 1);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  rnqnx::runtime::QtReactHost host(adapter);

  host.installTurboModuleBinding();
  host.installComponentProvider();
  host.installPlatformConstants();
  host.installTimers();

  if (!host.hasFabricUIManager()) {
    std::cerr << "RNQNX_REACT_QNX_ERROR fabric-ui-manager-missing\n";
    return 2;
  }

  if (!host.evaluate(
          "globalThis.__rnqnxQnxProbe = 6 * 7;",
          "rnqnx-react-qnx-probe.js")) {
    std::cerr << "RNQNX_REACT_QNX_ERROR hermes-eval "
              << host.lastError().toStdString() << '\n';
    return 3;
  }

  auto& runtime = host.runtime();
  const auto value =
      runtime.global().getProperty(runtime, "__rnqnxQnxProbe");
  if (!value.isNumber() || value.getNumber() != 42.0) {
    std::cerr << "RNQNX_REACT_QNX_ERROR hermes-result\n";
    return 4;
  }

  std::cout << "RNQNX_REACT_QNX_OK hermes fabric qtquick\n";
  return 0;
}
