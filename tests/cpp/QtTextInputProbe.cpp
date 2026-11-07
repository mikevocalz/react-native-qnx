// Synthetic Qt events through a real React TextInput, not a keyboard/IME engine.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/QtTextInputQml.h"
#include <react/renderer/core/RawEvent.h>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
QQuickItem* findEditor(QQuickItem* item) {
  if (item->objectName() == QStringLiteral("qnx-text-input")) return item;
  for (auto* child : item->childItems()) {
    if (auto* editor = findEditor(child)) return editor;
  }
  return nullptr;
}
unsigned editorCount(QQuickItem* item) {
  unsigned count = item->objectName() == QStringLiteral("qnx-text-input") ? 1U : 0U;
  for (auto* child : item->childItems()) count += editorCount(child);
  return count;
}
void run() {
  QQmlEngine engine;
  QQuickWindow window;
  window.resize(320, 240);
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray("import QtQuick\nItem { property color backgroundColor: 'black' }"));
  factory.registerType(QStringLiteral("TextInput"), QByteArray(rnqnx::views::textInputQml()));
  rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
  rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
  adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  adapter.mapComponent(QStringLiteral("TextInput"), QStringLiteral("TextInput"));
  rnqnx::runtime::QtReactHost host(adapter);
  host.installTurboModuleBinding(); host.installComponentProvider();
  host.installPlatformConstants(); host.installTimers();
  const char* overridePath = std::getenv("RNQNX_TEXT_INPUT_FIXTURE");
  const std::string path = overridePath ? overridePath : RNQNX_TEXT_INPUT_FIXTURE;
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "missing TextInput fixture");
  const std::string source((std::istreambuf_iterator<char>(file)), {});
  require(host.evaluate(source, path), "TextInput fixture evaluation failed");
  const bool intercept = std::getenv("RNQNX_TEXT_INPUT_INTERCEPT_TEST") != nullptr;
  auto listener = std::make_shared<const facebook::react::EventListener>([&](const facebook::react::RawEvent& event) {
    return intercept && (event.type == "change" || event.type == "topChange");
  });
  host.scheduler().addEventListener(listener);
  struct Guard {
    facebook::react::Scheduler& scheduler;
    std::shared_ptr<const facebook::react::EventListener> listener;
    ~Guard() { scheduler.removeEventListener(listener); }
  } guard{host.scheduler(), listener};
  const auto pump = [&] {
    host.runtime().drainMicrotasks(); host.pumpEvents();
    require(host.lastError().isEmpty(), "TextInput JavaScript runtime error");
    require(host.delegate().lastFailures().empty(), "TextInput Fabric mount failure");
  };
  const auto until = [&](auto predicate, const char* failure) {
    QElapsedTimer elapsed; elapsed.start();
    do { pump(); if (predicate()) return; QThread::msleep(1); } while (elapsed.elapsed() < 2000);
    require(predicate(), failure);
  };
  const auto settle = [&] { for (int i = 0; i < 20; ++i) pump(); };
  const auto snapshot = [&] { return host.runtime().global().getPropertyAsObject(host.runtime(), "__rnqnxTextInputSnapshot"); };
  const auto number = [&](const char* key) { return snapshot().getProperty(host.runtime(), key).asNumber(); };
  const auto text = [&] { return QString::fromStdString(snapshot().getProperty(host.runtime(), "text").asString(host.runtime()).utf8(host.runtime())); };
  // Invoke functions already compiled into the fixture HBC. Lean Hermes does
  // not accept inline source; runSync retains the owned JS entry/checkpoint.
  const auto call = [&](const char* name, auto&&... args) {
    host.taskQueue()->runSync([&] {
      auto& runtime = host.runtime();
      runtime.global().getPropertyAsFunction(runtime, name).call(runtime, args...);
    });
    settle();
  };
  window.show(); window.requestActivate();
  host.startSurface(11, "RNQnxTextInputProbe", 320, 240);
  until([&] { return layer.mountedCount() == 4 && editorCount(window.contentItem()) == 2; }, "React TextInput did not mount");
  QPointer<QQuickItem> field = findEditor(window.contentItem());
  require(field->property("text").toString() == QStringLiteral("A😀e\u0301"), "initial React value mismatch");
  require(number("changes") == 0, "initial JS value emitted native change");
  call("__rnqnxTextInputFocus");
  until([&] { return field->hasActiveFocus() && number("focus") >= 1; }, "React TextInput focus failed");
  const auto secondSnapshot = [&] { return host.runtime().global().getPropertyAsObject(host.runtime(), "__rnqnxTextInputSecondSnapshot"); };
  const auto secondCount = [&](const char* key) { return secondSnapshot().getProperty(host.runtime(), key).asNumber(); };
  const auto keyName = [&](const facebook::jsi::Object& state) {
    auto keys = state.getPropertyAsObject(host.runtime(), "keys").asArray(host.runtime());
    require(keys.size(host.runtime()) == 1, "traversal key count mismatch");
    return keys.getValueAtIndex(host.runtime(), 0).asString(host.runtime()).utf8(host.runtime());
  };
  QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
  QCoreApplication::sendEvent(&window, &tab);
  until([&] { return secondCount("focus") == 1 && number("blur") == 1; }, "Tab focus/blur did not reach React");
  auto* secondEditor = window.activeFocusItem();
  require(secondEditor && secondEditor != field && secondEditor->objectName() == QStringLiteral("qnx-text-input"),
          "Tab native focus target mismatch");
  require(keyName(snapshot()) == "Tab" && number("blurEventCount") == 0 && secondCount("focusEventCount") == 0,
          "Tab key or edit token mismatch");
  QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
  QCoreApplication::sendEvent(&window, &backtab);
  until([&] { return number("focus") == 2 && secondCount("blur") == 1; }, "Backtab focus/blur did not reach React");
  require(window.activeFocusItem() == field && keyName(secondSnapshot()) == "Tab" &&
          number("focusEventCount") == 0 && secondCount("blurEventCount") == 0 &&
          number("changes") == 0 && secondCount("changes") == 0 &&
          text() == QStringLiteral("A😀e\u0301") && secondEditor->property("text").toString() == QStringLiteral("second"),
          "Backtab changed native focus, text, or edit token");
  const auto blurBeforeReadonly = number("blur");
  const auto focusBeforeReadonly = number("focus");
  call("__rnqnxTextInputSetEditable", false);
  until([&] { return field->property("readOnly").toBool() && !field->hasActiveFocus(); }, "React editable=false did not release native focus");
  until([&] { return number("blur") == blurBeforeReadonly + 1; }, "React readOnly transition swallowed native blur");
  require(number("blurEventCount") == 0 && number("eventCount") == 0 && number("changes") == 0 &&
          text() == QStringLiteral("A😀e\u0301"), "readOnly blur changed text or edit token");
  call("__rnqnxTextInputSetEditable", true);
  until([&] { return !field->property("readOnly").toBool(); }, "React editable=true did not restore editing");
  call("__rnqnxTextInputFocus");
  until([&] { return field->hasActiveFocus() && number("focus") == focusBeforeReadonly + 1; }, "restored editable TextInput did not refocus");
  settle();
  require(number("blur") == blurBeforeReadonly + 1, "readOnly transition emitted duplicate blur");
  QKeyEvent key(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("X"));
  QCoreApplication::sendEvent(&window, &key);
  until([&] { return number("changes") == 1; }, "JavaScript TextInput change count mismatch");
  require(text() == QStringLiteral("A😀e\u0301X") && number("eventCount") == 1, "native text/count mismatch");
  require(field->property("cursorPosition").toInt() == 6, "UTF-16 key cursor mismatch");
  const auto assertSelection = [&] {
    const auto selection = snapshot().getPropertyAsObject(host.runtime(), "selection");
    require(selection.getProperty(host.runtime(), "start").asNumber() == field->property("selectionStart").toInt() &&
            selection.getProperty(host.runtime(), "end").asNumber() == field->property("selectionEnd").toInt() &&
            number("selectionEventCount") == number("eventCount"), "JavaScript native selection/count mismatch");
  };
  until([&] { return number("selectionEvents") >= 1 && number("selectionEventCount") == number("eventCount"); },
        "native selection did not reach React");
  assertSelection();
  call("__rnqnxTextInputFocusSecond");
  until([&] { return secondCount("focus") >= 1; }, "second React field focus failed");
  QKeyEvent other(QEvent::KeyPress, Qt::Key_Y, Qt::NoModifier, QStringLiteral("Y"));
  QCoreApplication::sendEvent(&window, &other);
  until([&] { return secondCount("changes") == 1; }, "second React field did not edit");
  require(secondCount("eventCount") == 1 && number("changes") == 1 && number("eventCount") == 1 &&
          text() == QStringLiteral("A😀e\u0301X"), "live TextInput tokens or state leaked across instances");
  require(secondSnapshot().getProperty(host.runtime(), "text").asString(host.runtime()).utf8(host.runtime()) == "secondY",
          "second React field text mismatch");
  call("__rnqnxTextInputFocus");
  until([&] { return field->hasActiveFocus(); }, "first React field refocus failed");
  QInputMethodEvent preedit(QStringLiteral("に"), {});
  QCoreApplication::sendEvent(&window, &preedit); settle();
  require(field->property("preeditText").toString() == QStringLiteral("に"), "Qt preedit did not reach focus item");
  require(number("changes") == 1 && number("eventCount") == 1, "preedit emitted committed change");
  QInputMethodEvent commit; commit.setCommitString(QStringLiteral("に"));
  QCoreApplication::sendEvent(&window, &commit);
  until([&] { return number("changes") == 2; }, "IME commit did not reach React TextInput");
  require(text() == QStringLiteral("A😀e\u0301Xに") && number("eventCount") == 2, "IME commit text/count mismatch");
  settle(); assertSelection();
  const auto nativeText = field->property("text").toString();
  const auto cursor = field->property("cursorPosition").toInt();
  call("__rnqnxTextInputCommand", 1, "stale", 0, 0);
  require(field->property("text").toString() == nativeText && field->property("cursorPosition").toInt() == cursor,
          "stale command changed text or selection");
  const auto selections = number("selectionEvents");
  call("__rnqnxTextInputCommand", 2, nullptr, 1, 3);
  require(field->property("selectedText").toString() == QStringLiteral("😀"), "command selection was not UTF-16");
  require(number("changes") == 2 && number("eventCount") == 2 && number("selectionEvents") == selections,
          "JS-origin selection emitted feedback");
  QInputMethodEvent replacement; replacement.setCommitString(QStringLiteral("好"));
  QCoreApplication::sendEvent(&window, &replacement);
  until([&] { return number("changes") == 3; }, "native replacement did not reach React");
  require(text() == QStringLiteral("A好e\u0301Xに") && number("eventCount") == 3, "native replacement mismatch");
  settle(); assertSelection();
  QInputMethodEvent pending(QStringLiteral("未"), {}); QCoreApplication::sendEvent(&window, &pending);
  QInputMethodEvent cancel(QString(), {}); QCoreApplication::sendEvent(&window, &cancel); settle();
  require(field->property("preeditText").toString().isEmpty() && number("changes") == 3, "cancel changed committed text");
  call("__rnqnxTextInputSetValue", "controlled");
  until([&] { return field->property("text").toString() == QStringLiteral("controlled"); }, "controlled JS value did not apply");
  require(number("changes") == 3 && number("eventCount") == 3, "controlled JS value echoed native change");
  call("__rnqnxTextInputBlur");
  until([&] { return number("blur") >= 1; }, "React TextInput blur failed");
  require(number("blurEventCount") == 3, "blur advanced edit token");
  QPointer<QQuickItem> previous(field);
  auto* queued = new QInputMethodEvent; queued->setCommitString(QStringLiteral("late"));
  QCoreApplication::postEvent(field, queued);
  host.stopSurface(11);
  require(previous.isNull(), "surface stop retained editor"); settle();
  require(number("changes") == 3, "queued event reached deleted React input");
  host.startSurface(11, "RNQnxTextInputProbe", 320, 240);
  until([&] { return layer.mountedCount() == 4 && editorCount(window.contentItem()) == 2; }, "TextInput remount failed");
  field = findEditor(window.contentItem());
  call("__rnqnxTextInputFocus");
  QKeyEvent again(QEvent::KeyPress, Qt::Key_Z, Qt::NoModifier, QStringLiteral("Z"));
  QCoreApplication::sendEvent(&window, &again);
  until([&] { return number("changes") == 1; }, "remounted TextInput failed to edit");
  host.stopSurface(11); settle();
  std::cout << "RNQNX_TEXT_INPUT_OK react key preedit commit utf16 stale selection no-echo focus teardown remount\n";
}
}
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try { run(); return 0; }
  catch (const std::exception& error) { std::cerr << "RNQNX_TEXT_INPUT_FAIL " << error.what() << '\n'; return 1; }
}
