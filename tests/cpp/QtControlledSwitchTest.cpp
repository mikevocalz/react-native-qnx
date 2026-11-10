// Real Qt MouseArea -> bridge -> adapter command behavior. This is not a
// running React consumer or a physical input qualification.
#include "react-native-qnx/fabric/QtSwitchBridge.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/QtSwitchQml.h"

#include <react/renderer/components/FBReactNativeSpec/Props.h>
#include <folly/dynamic.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQmlEngine>
#include <QQuickWindow>

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace rnqnx::fabric;
using facebook::react::ShadowView;
using facebook::react::ShadowViewMutation;

void expect(bool condition, const char* detail) {
  if (!condition) throw std::runtime_error(detail);
}

struct Fixture {
  QQmlEngine engine;
  QQuickWindow window;
  rnqnx::views::QtQuickItemFactory factory{engine};
  QtQuickMountingLayer layer{factory, *window.contentItem(), 1};
  ShadowViewMutationAdapter adapter{layer};
  QQuickItem* item{nullptr};
  QtSwitchBridge* bridge{nullptr};
  std::vector<bool> events;

  Fixture() {
    factory.registerType("Switch", rnqnx::views::switchQml());
    adapter.mapComponent("Switch", "Switch");
    ShadowView view;
    view.tag = 2;
    view.componentName = "Switch";
    view.props = std::make_shared<facebook::react::SwitchProps>();
    view.layoutMetrics.frame.size = {80, 40};
    expect(adapter.apply({ShadowViewMutation::CreateMutation(view),
        ShadowViewMutation::InsertMutation(1, view, 0)}).empty(), "mount Switch");
    item = layer.itemForTag(2);
    bridge = item->findChild<QtSwitchBridge*>();
    expect(bridge != nullptr, "native event bridge");
    bridge->setOnChange([this](bool value) { events.push_back(value); });
    window.showFullScreen();
    QCoreApplication::processEvents();
  }

  bool value() const { return item->property("value").toBool(); }

  void click() {
    const auto point = item->mapToScene(QPointF(30, 20));
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
      QMouseEvent event(type, point, QPointF(window.mapToGlobal(point.toPoint())), Qt::LeftButton,
          type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(&window, &event);
    }
  }

  void command(bool value) {
    expect(adapter.dispatchCommand(2, "setValue", folly::dynamic::array(value), nullptr).error == MountError::None,
        "setValue command");
  }
};

void controlledChanges() {
  Fixture f;
  f.click();
  expect(f.value() && f.events == std::vector<bool>{true}, "one native proposal");
  f.command(false); // Rejected by React; no Fabric props delta is required.
  expect(!f.value() && f.events.size() == 1, "rejection reconciles without echo");
  f.click();
  expect(f.value() && f.events == std::vector<bool>({true, true}), "same proposal after rejection emits again");
  f.command(true); // Accepted.
  expect(f.events.size() == 2, "acceptance does not duplicate callback");
  f.click();
  f.command(true); // Deferred acceptance: keep old controlled value first.
  expect(f.value(), "deferred proposal initially restored");
  f.command(false); // Later application decision.
  expect(!f.value() && f.events.size() == 3, "deferred decision has no callback");
  for (int i = 0; i < 20; ++i) f.click();
  expect(!f.value() && f.events.size() == 23, "rapid alternating clicks delivered exactly once");
  for (std::size_t i = 3; i < f.events.size(); ++i) expect(f.events[i] == (i % 2 == 1), "ordered alternating proposals");
  f.bridge->notify();
  expect(f.events.size() == 23, "duplicate native notification suppressed");
}

void disabledExternalAndInvalidCommands() {
  Fixture f;
  SwitchStyle style;
  style.disabled = true;
  style.value = false;
  expect(f.layer.updateSwitch(2, style) == MountError::None, "disabled prop update");
  f.click();
  expect(!f.value() && f.events.empty(), "disabled native Switch never proposes");
  style.value = true;
  expect(f.layer.updateSwitch(2, style) == MountError::None, "external disabled update");
  expect(f.value() && f.events.empty(), "external writes never emit even without prearming bridge");
  f.command(false);
  expect(!f.value() && f.events.empty(), "setValue works while disabled");
  style.disabled = false;
  style.value = true;
  f.bridge->setExpectedValue(true);
  expect(f.layer.updateSwitch(2, style) == MountError::None, "enable and external update");
  f.click();
  expect(!f.value() && f.events == std::vector<bool>{false}, "next user event follows external value");
  for (const auto& args : {folly::dynamic::array(), folly::dynamic::array(1),
       folly::dynamic::array(true, false), folly::dynamic(nullptr)}) {
    expect(f.adapter.dispatchCommand(2, "setValue", args, nullptr).error != MountError::None, "bad command rejected");
    expect(!f.value(), "invalid command does not mutate");
  }
  expect(f.adapter.dispatchCommand(999, "setValue", folly::dynamic::array(true), nullptr).error == MountError::UnknownTag,
      "unknown tag rejected");
  expect(f.layer.destroy(2) == MountError::None, "destroy Switch");
  expect(f.adapter.dispatchCommand(2, "setValue", folly::dynamic::array(true), nullptr).error == MountError::UnknownTag,
      "destroyed Switch command rejected without dereferencing stale bridge");
}

void synchronousRejectionAndDestruction() {
  Fixture f;
  int callbacks = 0;
  f.bridge->setOnChange([&](bool) { ++callbacks; f.command(false); });
  f.click();
  expect(callbacks == 1 && !f.value(), "callback can synchronously reject without recursion or lock");
  f.bridge->setOnChange([&](bool) { ++callbacks; f.layer.destroy(2); });
  // Exercise slot lifetime separately from the QML mouse-release stack.
  f.item->setProperty("value", true);
  f.bridge->notify();
  expect(callbacks == 2 && f.layer.itemForTag(2) == nullptr, "callback may destroy native bridge");
}
} // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    controlledChanges();
    disabledExternalAndInvalidCommands();
    synchronousRejectionAndDestruction();
    std::cout << "RNQNX_CONTROLLED_SWITCH_OK\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
