// Linux host evidence for Qt focus -> Fabric -> public RN callbacks, including
// actual Pressability keyboard activation and disconnected native lifetimes.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtViewFocusBridge.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using rnqnx::fabric::QtViewFocusBridge;
using rnqnx::runtime::QtReactHost;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void evaluate(QtReactHost& host, const char* script) {
  if (!host.evaluate(script, "view-focus-test.js")) {
    throw std::runtime_error(host.lastError().toStdString());
  }
}
void flush(QtReactHost& host) {
  for (int i = 0; i < 20; ++i) {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
  }
}
void settle(QtReactHost& host, const char* phase, const std::function<bool()>& ready) {
  QElapsedTimer deadline;
  deadline.start();
  while (!ready() && deadline.elapsed() < 3000) host.pumpEvents();
  if (!ready()) throw std::runtime_error(std::string("Focus fixture did not settle: ") + phase);
}
QQuickItem* find(QQuickItem& root, const char* testId) {
  if (root.property("testID").toString() == QString::fromUtf8(testId)) return &root;
  for (auto* child : root.childItems()) {
    if (auto* item = find(*child, testId)) return item;
  }
  return nullptr;
}
void key(QQuickWindow& window, QEvent::Type type, int code,
    bool repeated = false, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent event(type, code, modifiers, QString{}, repeated);
  QCoreApplication::sendEvent(&window, &event);
}
void clickKey(QQuickWindow& window, int code) {
  key(window, QEvent::KeyPress, code);
  key(window, QEvent::KeyRelease, code);
}

void receiverLifetime() {
  QQuickWindow window;
  window.resize(320, 240);
  auto item = std::make_unique<QQuickItem>(window.contentItem());
  item->setSize(QSizeF(100, 40));
  window.show();
  window.requestActivate();
  QCoreApplication::processEvents();
  int first = 0;
  int latest = 0;
  int presses = 0;
  auto* bridge = new QtViewFocusBridge(*item);
  QPointer<QtViewFocusBridge> receiver(bridge);
  bridge->update([&](bool) { ++first; }, [&] { ++presses; }, true);
  bridge->setMounted(true);
  item->forceActiveFocus();
  require(first == 1, "Generic native focus was not observed exactly once");
  bridge->update([&](bool) { ++latest; }, [&] { ++presses; }, true);
  item->setFocus(false);
  require(first == 1 && latest == 1, "Updated view retained a stale or duplicate receiver");
  bridge->setMounted(false);
  require(!item->activeFocusOnTab(), "Detached item remained in the tab sequence");
  item->forceActiveFocus();
  clickKey(window, Qt::Key_Return);
  item->setFocus(false);
  require(first == 1 && latest == 1 && presses == 0,
      "Detached receiver delivered focus or activation");
  bridge->setMounted(true);
  item->forceActiveFocus();
  require(latest == 2, "Reinserted receiver did not reconnect");
  bridge->setMounted(false);
  delete bridge;
  require(receiver.isNull(), "Deleted receiver was retained");
  item->setFocus(false);
  item->forceActiveFocus();
  clickKey(window, Qt::Key_Space);
  require(latest == 2 && presses == 0, "Deleted receiver remained connected");
  bridge = new QtViewFocusBridge(*item);
  receiver = bridge;
  bridge->update([&](bool) { ++latest; }, [&] { ++presses; }, true);
  bridge->setMounted(true);
  item.reset();
  require(receiver.isNull(), "Native item destruction did not own its receiver");
  QCoreApplication::processEvents();
}

void publicCallbacks() {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(640, 480);
  window.show();
  window.requestActivate();
  QCoreApplication::processEvents();
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(
      "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n"));
  constexpr facebook::react::SurfaceId surface = 41;
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), surface);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  adapter.mapComponent(QStringLiteral("ModalHostView"), QStringLiteral("View"));
  QtReactHost host(adapter);
  require(host.observeWindowMetrics(surface, window, *window.contentItem()), "Window binding failed");
  host.installTurboModuleBinding();
  host.installComponentProvider();
  host.installPlatformConstants();
  host.installTimers();
  std::ifstream input(RNQNX_VIEW_FOCUS_BUNDLE_PATH);
  require(static_cast<bool>(input), "Focus bundle is missing");
  std::stringstream source;
  source << input.rdbuf();
  require(host.evaluate(source.str(), RNQNX_VIEW_FOCUS_BUNDLE_PATH), "Focus bundle evaluation failed");
  host.startSurface(surface, "RNQnxViewFocus", 640, 480);
  settle(host, "initial button mount", [&] { return find(*window.contentItem(), "focus-button") != nullptr; });
  QPointer<QQuickItem> button = find(*window.contentItem(), "focus-button");
  auto* plain = find(*window.contentItem(), "focus-plain");
  require(button->activeFocusOnTab() && plain && !plain->activeFocusOnTab(),
      "Tab eligibility did not follow the explicit button role");
  evaluate(host, "__focusButton();");
  flush(host);
  require(button->hasActiveFocus(), "Public View ref focus did not reach Qt");
  evaluate(host, "if (__viewFocusProbe.events.join(',') !== 'button-focus') throw Error('public onFocus');");

  clickKey(window, Qt::Key_Return);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 1) throw Error('Enter must press once');");
  key(window, QEvent::KeyPress, Qt::Key_Space);
  for (int i = 0; i < 3; ++i) {
    key(window, QEvent::KeyRelease, Qt::Key_Space, true);
    key(window, QEvent::KeyPress, Qt::Key_Space, true);
  }
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 1) throw Error('autorepeat activated');");
  key(window, QEvent::KeyRelease, Qt::Key_Space);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 2) throw Error('Space must press once');");

  key(window, QEvent::KeyPress, Qt::Key_Space);
  evaluate(host, "__focusPlain();");
  flush(host);
  evaluate(host, "if (__viewFocusProbe.events.slice(-2).join(',') !== 'button-blur,plain-focus') throw Error('public blur/focus');");
  evaluate(host, "__focusButton();");
  flush(host);
  key(window, QEvent::KeyRelease, Qt::Key_Space);
  clickKey(window, Qt::Key_Return);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 3) throw Error('focus loss failed to cancel held key');");

  key(window, QEvent::KeyPress, Qt::Key_Space);
  evaluate(host, "__setFocusProbe({disabled:true});");
  settle(host, "disabled button leaves tab sequence", [&] { return !button->activeFocusOnTab(); });
  key(window, QEvent::KeyRelease, Qt::Key_Space);
  clickKey(window, Qt::Key_Return);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 3) throw Error('disabled button activated'); __setFocusProbe({disabled:false,generation:1});");
  settle(host, "enabled button returns to tab sequence", [&] { return button->activeFocusOnTab(); });
  evaluate(host, "__focusButton();");
  flush(host);
  key(window, QEvent::KeyRelease, Qt::Key_Space);
  key(window, QEvent::KeyPress, Qt::Key_Return, false, Qt::ControlModifier);
  key(window, QEvent::KeyRelease, Qt::Key_Return, false, Qt::ControlModifier);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 3) throw Error('cancelled or modified key activated');");
  clickKey(window, Qt::Key_Enter);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 4 || __viewFocusProbe.generations[3] !== 1) throw Error('latest React callback was not used');");

  key(window, QEvent::KeyPress, Qt::Key_Space);
  evaluate(host, "__setFocusProbe({shown:false});");
  settle(host, "removed button is destroyed", [&] { return button.isNull(); });
  require(find(*window.contentItem(), "focus-button") == nullptr, "Removed button remained mounted");
  evaluate(host, "__setFocusProbe({shown:true});");
  settle(host, "button remount", [&] { return find(*window.contentItem(), "focus-button") != nullptr; });
  evaluate(host, "__focusButton();");
  flush(host);
  key(window, QEvent::KeyRelease, Qt::Key_Space);
  flush(host);
  evaluate(host, "if (__viewFocusProbe.presses !== 4) throw Error('removed view leaked armed activation');");

  evaluate(host, "__setFocusProbe({modal:1});");
  settle(host, "first modal initial focus", [&] {
    auto* item = find(*window.contentItem(), "outer-first");
    return item && item->hasActiveFocus();
  });
  clickKey(window, Qt::Key_Tab);
  flush(host);
  require(find(*window.contentItem(), "outer-second")->hasActiveFocus(), "Modal Tab did not stay inside");
  key(window, QEvent::KeyPress, Qt::Key_Backtab, false, Qt::ShiftModifier);
  key(window, QEvent::KeyRelease, Qt::Key_Backtab, false, Qt::ShiftModifier);
  flush(host);
  require(find(*window.contentItem(), "outer-first")->hasActiveFocus(), "Modal Backtab did not stay inside");
  evaluate(host, "__focusButton();");
  flush(host);
  clickKey(window, Qt::Key_Space);
  flush(host);
  require(find(*window.contentItem(), "outer-first")->hasActiveFocus(), "Escaped focus was not returned to the modal");
  evaluate(host, "if (__viewFocusProbe.presses !== 4) throw Error('keyboard activated behind modal'); __focusOuterSecond(); __setFocusProbe({modal:2});");
  settle(host, "nested modal initial focus", [&] {
    auto* item = find(*window.contentItem(), "inner-first");
    return item && item->hasActiveFocus();
  });
  clickKey(window, Qt::Key_Tab);
  flush(host);
  require(find(*window.contentItem(), "inner-second")->hasActiveFocus(), "Nested modal did not own Tab");
  clickKey(window, Qt::Key_Tab);
  flush(host);
  require(find(*window.contentItem(), "inner-first")->hasActiveFocus(), "Nested modal tab wrapping escaped or bounced");
  clickKey(window, Qt::Key_Escape);
  settle(host, "nested modal restores previous outer focus", [&] {
    auto* item = find(*window.contentItem(), "outer-second");
    return !find(*window.contentItem(), "inner-first") && item && item->hasActiveFocus();
  });
  clickKey(window, Qt::Key_Escape);
  settle(host, "last modal restores previous application focus", [&] {
    auto* item = find(*window.contentItem(), "focus-button");
    return !find(*window.contentItem(), "outer-first") && item && item->hasActiveFocus();
  });
  require(host.delegate().lastFailures().empty(), "Focus fixture had mounting failures");
  host.stopSurface(surface);
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    receiverLifetime();
    publicCallbacks();
    std::cout << "RNQNX_VIEW_FOCUS_OK public focus blur keyboard press repeat disable cancellation modal nested-modal teardown\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
