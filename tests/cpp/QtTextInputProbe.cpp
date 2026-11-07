// Synthetic Qt events through a real React TextInput, not a keyboard/IME engine.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
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
  const auto script = [&](const char* code) { require(host.evaluate(code, "text-input-driver"), "TextInput driver failed"); settle(); };
  window.show(); window.requestActivate();
  host.startSurface(11, "RNQnxTextInputProbe", 320, 240);
  until([&] { return window.contentItem()->findChild<QQuickItem*>("qnx-text-input") != nullptr; }, "React TextInput did not mount");
  QPointer<QQuickItem> field = window.contentItem()->findChild<QQuickItem*>("qnx-text-input");
  require(field->property("text").toString() == QStringLiteral("A😀e\u0301"), "initial React value mismatch");
  require(number("changes") == 0, "initial JS value emitted native change");
  script("__rnqnxTextInputFocus()");
  until([&] { return field->hasActiveFocus() && number("focus") >= 1; }, "React TextInput focus failed");
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
  script("__rnqnxTextInputFocusSecond()");
  const auto secondSnapshot = [&] { return host.runtime().global().getPropertyAsObject(host.runtime(), "__rnqnxTextInputSecondSnapshot"); };
  const auto secondCount = [&](const char* key) { return secondSnapshot().getProperty(host.runtime(), key).asNumber(); };
  until([&] { return secondCount("focus") >= 1; }, "second React field focus failed");
  QKeyEvent other(QEvent::KeyPress, Qt::Key_Y, Qt::NoModifier, QStringLiteral("Y"));
  QCoreApplication::sendEvent(&window, &other);
  until([&] { return secondCount("changes") == 1; }, "second React field did not edit");
  require(secondCount("eventCount") == 1 && number("changes") == 1 && number("eventCount") == 1 &&
          text() == QStringLiteral("A😀e\u0301X"), "live TextInput tokens or state leaked across instances");
  require(secondSnapshot().getProperty(host.runtime(), "text").asString(host.runtime()).utf8(host.runtime()) == "secondY",
          "second React field text mismatch");
  script("__rnqnxTextInputFocus()");
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
  script("__rnqnxTextInputCommand(1, 'stale', 0, 0)");
  require(field->property("text").toString() == nativeText && field->property("cursorPosition").toInt() == cursor,
          "stale command changed text or selection");
  const auto selections = number("selectionEvents");
  script("__rnqnxTextInputCommand(2, null, 1, 3)");
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
  script("__rnqnxTextInputSetValue('controlled')");
  until([&] { return field->property("text").toString() == QStringLiteral("controlled"); }, "controlled JS value did not apply");
  require(number("changes") == 3 && number("eventCount") == 3, "controlled JS value echoed native change");
  script("__rnqnxTextInputBlur()");
  until([&] { return number("blur") >= 1; }, "React TextInput blur failed");
  require(number("blurEventCount") == 3, "blur advanced edit token");
  QPointer<QQuickItem> previous(field);
  auto* queued = new QInputMethodEvent; queued->setCommitString(QStringLiteral("late"));
  QCoreApplication::postEvent(field, queued);
  host.stopSurface(11);
  require(previous.isNull(), "surface stop retained editor"); settle();
  require(number("changes") == 3, "queued event reached deleted React input");
  host.startSurface(11, "RNQnxTextInputProbe", 320, 240);
  until([&] { return window.contentItem()->findChild<QQuickItem*>("qnx-text-input") != nullptr; }, "TextInput remount failed");
  field = window.contentItem()->findChild<QQuickItem*>("qnx-text-input");
  script("__rnqnxTextInputFocus()");
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
