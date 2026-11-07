#include "react-native-qnx/fabric/QtTextInputBridge.h"
#include "react-native-qnx/views/QtTextInputQml.h"
#if RNQNX_TEXT_INPUT_MOUNTING_TEST
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#endif
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QKeyEvent>
#include <QPointer>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace rnqnx::fabric;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct ClearEmitter {
  QPointer<QtTextInputBridge> bridge;
  ~ClearEmitter() { if (bridge) bridge->setEmitter({}); }
};
// A real window dispatches keys to two independently owned FocusScopes.
void checkKeyboardTraversal(QQmlComponent& component) {
  std::vector<QString> keys;
  unsigned secondBlurs = 0;
  QQuickWindow window;
  window.resize(320, 240);
  std::unique_ptr<QObject> firstObject(component.create());
  std::unique_ptr<QObject> secondObject(component.create());
  auto* first = qobject_cast<QQuickItem*>(firstObject.get());
  auto* second = qobject_cast<QQuickItem*>(secondObject.get());
  require(first && second, "focus wrappers unavailable");
  first->setParentItem(window.contentItem()); second->setParentItem(window.contentItem());
  first->setSize(QSizeF(120, 40)); second->setSize(QSizeF(120, 40)); second->setY(50);
  auto* firstBridge = new QtTextInputBridge(*first);
  auto* secondBridge = new QtTextInputBridge(*second);
  require(firstBridge->configure(false, false) && secondBridge->configure(false, false), "focus configuration failed");
  auto* firstEditor = first->findChild<QQuickItem*>("qnx-text-input");
  auto* secondEditor = second->findChild<QQuickItem*>("qnx-text-input");
  require(first->activeFocusOnTab() && second->activeFocusOnTab() &&
          !firstEditor->activeFocusOnTab() && !secondEditor->activeFocusOnTab() &&
          !first->findChild<QQuickItem*>("qnx-text-edit")->activeFocusOnTab(), "editor has duplicate or missing tab stop");
  firstBridge->setEmitter([&](TextInputEvent event, const TextInputObservation& value) {
    if (event == TextInputEvent::Key) keys.push_back(value.key);
  });
  secondBridge->setEmitter([&](TextInputEvent event, const TextInputObservation& value) {
    if (event == TextInputEvent::Blur) { ++secondBlurs; require(value.eventCount == 0, "readOnly blur advanced edit token"); }
    if (event == TextInputEvent::Key) keys.push_back(value.key);
  });
  ClearEmitter clearFirst{firstBridge};
  ClearEmitter clearSecond{secondBridge};
  window.show(); window.requestActivate(); first->forceActiveFocus();
  const auto pump = [] { for (int i = 0; i < 10; ++i) QCoreApplication::processEvents(); };
  pump();
  require(window.activeFocusItem() == firstEditor, "initial native focus mismatch");
  QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
  QCoreApplication::sendEvent(&window, &tab); pump();
  require(window.activeFocusItem() == secondEditor, "Tab did not reach second editor");
  QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
  QCoreApplication::sendEvent(&window, &backtab); pump();
  require(window.activeFocusItem() == firstEditor, "Backtab did not reach first editor");
  require(keys.size() == 2 && keys[0] == QStringLiteral("\t") && keys[1] == QStringLiteral("\t"), "Tab/Backtab key mapping mismatch");
  for (const auto state : {0, 1, 2}) {
    if (state == 0) second->setProperty("readOnly", true);
    if (state == 1) { second->setProperty("readOnly", false); second->setEnabled(false); }
    if (state == 2) { second->setEnabled(true); second->setVisible(false); }
    QKeyEvent skip(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
    QCoreApplication::sendEvent(&window, &skip); pump();
    require(window.activeFocusItem() == firstEditor, "Tab reached noneditable/disabled/hidden field");
  }
  second->setVisible(true); second->forceActiveFocus(); pump();
  const auto blursBeforeReadonly = secondBlurs;
  secondBridge->beginJSUpdate();
  second->setProperty("readOnly", true);
  secondBridge->endJSUpdate(); pump();
  require(secondBlurs == blursBeforeReadonly + 1, "JS readOnly transition swallowed native blur");
  require(!second->hasActiveFocus() && !second->activeFocusOnTab(), "readOnly transition retained focus/tab eligibility");
  second->setProperty("readOnly", false); second->forceActiveFocus(); pump();
  secondBridge->invalidate(); pump();
  require(!second->hasActiveFocus() && !second->activeFocusOnTab(), "invalidated session retained focus/tab eligibility");
  require(firstBridge->eventCount() == 0 && secondBridge->eventCount() == 0, "focus traversal edited text");
}
// Exercise the documented callback-can-delete contract during JS-owned updates.
void checkBlurDeletion(QQmlComponent& component) {
  for (const bool secureMultiline : {false, true}) {
    unsigned blurs = 0;
    QQuickWindow window;
    window.resize(320, 240);
    std::unique_ptr<QObject> object(component.create());
    auto* wrapper = qobject_cast<QQuickItem*>(object.get());
    require(wrapper, "deletion wrapper unavailable");
    wrapper->setParentItem(window.contentItem()); wrapper->setSize(QSizeF(120, 40));
    QPointer<QtTextInputBridge> bridge = new QtTextInputBridge(*wrapper);
    require(bridge->configure(false, false), "deletion editor unavailable");
    ClearEmitter clear{bridge};
    window.show(); window.requestActivate(); wrapper->forceActiveFocus();
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
    require(wrapper->hasActiveFocus(), "deletion editor did not focus");
    bridge->setEmitter([&](TextInputEvent event, const TextInputObservation&) {
      if (event == TextInputEvent::Blur) { ++blurs; object.reset(); }
    });
    QPointer<QQuickItem> previous(wrapper);
    bridge->beginJSUpdate();
    if (secureMultiline) {
      require(!bridge->configure(true, true), "deleted secure configuration succeeded");
    } else {
      wrapper->setProperty("readOnly", true);
    }
    if (bridge) bridge->endJSUpdate();
    require(previous.isNull() && bridge.isNull() && blurs == 1, "blur callback deletion was suppressed or retained target");
  }
}
void checkFocusTransactions(QQmlComponent& component) {
  std::vector<TextInputEvent> events;
  std::vector<QString> snapshots;
  QQuickWindow window;
  window.resize(320, 240);
  std::unique_ptr<QObject> object(component.create());
  auto* wrapper = qobject_cast<QQuickItem*>(object.get());
  require(wrapper, "transaction wrapper unavailable");
  wrapper->setParentItem(window.contentItem()); wrapper->setSize(QSizeF(120, 40));
  QPointer<QtTextInputBridge> bridge = new QtTextInputBridge(*wrapper);
  ClearEmitter clear{bridge};
  require(bridge->configure(false, false), "transaction editor unavailable");
  window.show(); window.requestActivate(); wrapper->forceActiveFocus();
  for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
  require(wrapper->hasActiveFocus(), "transaction editor did not focus");
  bool nested = false;
  bridge->setEmitter([&](TextInputEvent kind, const TextInputObservation& value) {
    events.push_back(kind); snapshots.push_back(value.text);
    require(value.eventCount == 0, "focus transaction advanced edit count");
    if (!nested) {
      nested = true;
      bridge->beginJSUpdate(); wrapper->forceActiveFocus(); bridge->endJSUpdate();
    }
  });
  bridge->beginJSUpdate();
  wrapper->setFocus(false);
  wrapper->forceActiveFocus();
  bridge->beginJSUpdate(); wrapper->setFocus(false); bridge->endJSUpdate();
  require(events.empty(), "nested JS scope delivered focus inside Qt setters");
  require(bridge->applyJS(0, QStringLiteral("JS"), 0, 0), "transaction JS value failed");
  bridge->endJSUpdate();
  const std::vector<TextInputEvent> expected{TextInputEvent::Blur, TextInputEvent::Focus, TextInputEvent::Blur, TextInputEvent::Focus};
  require(events == expected && bridge->eventCount() == 0, "focus transaction FIFO/reentrant ordering or no-echo mismatch");
  require(snapshots.size() == 4 && snapshots[0].isEmpty() && snapshots[1].isEmpty() && snapshots[2].isEmpty() &&
          snapshots[3] == QStringLiteral("JS"), "focus payload was resampled instead of captured at transition");
  // A throwing observer propagates from explicit finish, clears the remaining
  // queue, and leaves the bridge usable for a subsequent owned transaction.
  events.clear();
  bridge->setEmitter([&](TextInputEvent kind, const TextInputObservation&) {
    events.push_back(kind);
    throw std::runtime_error("synthetic focus observer failure");
  });
  bridge->beginJSUpdate(); wrapper->setFocus(false); wrapper->forceActiveFocus();
  bool threw = false;
  try { bridge->endJSUpdate(); }
  catch (const std::runtime_error& error) { threw = std::string(error.what()) == "synthetic focus observer failure"; }
  require(threw && events == std::vector<TextInputEvent>{TextInputEvent::Blur}, "focus observer exception was hidden or queue replayed");
  bridge->setEmitter([&](TextInputEvent kind, const TextInputObservation&) { events.push_back(kind); });
  bridge->beginJSUpdate(); wrapper->setFocus(false); bridge->cancelJSUpdate();
  require(events.size() == 1, "exception cancellation invoked observer");
  bridge->beginJSUpdate(); wrapper->forceActiveFocus(); bridge->endJSUpdate();
  require(events.size() == 2 && events.back() == TextInputEvent::Focus, "focus drain did not recover after throw/cancel");
  events.clear();
  bridge->beginJSUpdate(); wrapper->setFocus(false); bridge->invalidate(); bridge->endJSUpdate();
  require(events.empty(), "invalidated transaction retained queued focus observation");
}
#if RNQNX_TEXT_INPUT_MOUNTING_TEST
void checkMountedBlurDeletion(QQmlEngine& engine) {
  for (const bool secureMultiline : {false, true}) {
    unsigned blurs = 0;
    QQuickWindow window;
    window.resize(320, 240);
    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("TextInput"), QByteArray(rnqnx::views::textInputQml()));
    QtQuickMountingLayer layer(factory, *window.contentItem(), 100);
    require(layer.create(101, QStringLiteral("TextInput")) == MountError::None &&
            layer.insert(100, 101, 0) == MountError::None, "mounted deletion editor unavailable");
    TextInputStyle style;
    require(layer.updateTextInput(101, style) == MountError::None, "mounted deletion configuration failed");
    QPointer<QQuickItem> item = layer.itemForTag(101);
    item->setSize(QSizeF(120, 40));
    QPointer<QtTextInputBridge> bridge = layer.textInputBridge(101);
    ClearEmitter clear{bridge};
    window.show(); window.requestActivate(); item->forceActiveFocus();
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
    require(item->hasActiveFocus(), "mounted deletion editor did not focus");
    bridge->setEmitter([&](TextInputEvent event, const TextInputObservation&) {
      if (event == TextInputEvent::Blur) { ++blurs; require(layer.destroy(101) == MountError::None, "blur deletion failed"); }
    });
    style.readOnly = true; style.secure = secureMultiline; style.multiline = secureMultiline;
    const auto result = layer.updateTextInput(101, style);
    require(result == (secureMultiline ? MountError::UnsupportedTextInput : MountError::UnknownTag) &&
            item.isNull() && bridge.isNull() && blurs == 1 && layer.mountedCount() == 1 &&
            layer.lastError() == (secureMultiline ? QStringLiteral("Secure multiline TextInput is unsupported") : QStringLiteral("TextInput target removed during update")),
            "mount update accessed or retained deleted blur target");
  }
}
void checkMountedThrowAndTeardown(QQmlEngine& engine) {
  unsigned teardownCallbacks = 0;
  QQuickWindow window;
  window.resize(320, 240);
  rnqnx::views::QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("TextInput"), QByteArray(rnqnx::views::textInputQml()));
  QPointer<QtTextInputBridge> previous;
  {
    QtQuickMountingLayer layer(factory, *window.contentItem(), 100);
    require(layer.create(101, QStringLiteral("TextInput")) == MountError::None && layer.insert(100, 101, 0) == MountError::None, "throw editor unavailable");
    TextInputStyle style;
    require(layer.updateTextInput(101, style) == MountError::None, "throw editor configuration failed");
    auto* item = layer.itemForTag(101); item->setSize(QSizeF(120, 40));
    previous = layer.textInputBridge(101);
    window.show(); window.requestActivate(); item->forceActiveFocus();
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
    require(item->hasActiveFocus(), "throw editor did not focus");
    previous->setEmitter([](TextInputEvent kind, const TextInputObservation&) {
      if (kind == TextInputEvent::Blur) throw std::runtime_error("synthetic mounted observer failure");
    });
    style.readOnly = true;
    bool threw = false;
    try { layer.updateTextInput(101, style); }
    catch (const std::runtime_error& error) { threw = std::string(error.what()) == "synthetic mounted observer failure"; }
    require(threw && previous, "mounted observer did not throw from explicit finish");
    previous->setEmitter({}); style.readOnly = false;
    require(layer.updateTextInput(101, style) == MountError::None, "mounted scope did not recover after observer throw");
    item->forceActiveFocus();
    // This callback stays armed across the layer destructor. Its captured state
    // outlives the layer, and it does not mutate the registry on failure.
    previous->setEmitter([&](TextInputEvent, const TextInputObservation&) { ++teardownCallbacks; });
  }
  require(previous.isNull() && teardownCallbacks == 0, "layer teardown admitted TextInput callback");
}

#endif
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(rnqnx::views::textInputQml(), QUrl());
    require(component.isReady(), "TextInput QML unavailable");
    checkKeyboardTraversal(component);
    checkBlurDeletion(component);
    checkFocusTransactions(component);
#if RNQNX_TEXT_INPUT_MOUNTING_TEST
    checkMountedBlurDeletion(engine);
    checkMountedThrowAndTeardown(engine);
#endif
    std::vector<TextInputObservation> changes;
    std::vector<TextInputObservation> selections;
    std::unique_ptr<QObject> object(component.create());
    auto* wrapper = qobject_cast<QQuickItem*>(object.get());
    require(wrapper != nullptr, "TextInput wrapper unavailable");
    auto* input = wrapper->findChild<QQuickItem*>("qnx-text-input");
    require(input != nullptr, "TextInput child unavailable");
    auto* bridge = new QtTextInputBridge(*wrapper);
    bridge->setEmitter([&](TextInputEvent event, const TextInputObservation& value) {
      if (event == TextInputEvent::Change) {
        changes.push_back(value);
        bridge->applyJS(value.eventCount, value.text); // Synchronous unchanged JS acknowledgment.
      }
      if (event == TextInputEvent::Selection) selections.push_back(value);
    });
    ClearEmitter clearMain{bridge};
    require(bridge->configure(false, false), "ordinary TextInput unavailable");
    require(bridge->applyJS(0, QStringLiteral("A😀e\u0301")), "initial JS write rejected");
    require(bridge->eventCount() == 0 && changes.empty(), "JS-origin change feedback");
    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("X"));
    QCoreApplication::sendEvent(input, &commit);
    require(bridge->eventCount() == 1 && changes.size() == 1, "native commit count mismatch");
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
    require(changes.back().text == QStringLiteral("A😀e\u0301X"), "native UTF-8/UTF-16 text mismatch");
    if (selections.empty() || selections.back().selectionStart != 6 || selections.back().selectionEnd != 6) {
      std::cerr << "selection observations=" << selections.size()
                << " native=" << input->property("selectionStart").toInt() << ',' << input->property("selectionEnd").toInt();
      for (const auto& value : selections) std::cerr << " observed=" << value.selectionStart << ',' << value.selectionEnd << ':' << value.eventCount;
      std::cerr << '\n';
    }
    require(!selections.empty() && selections.back().selectionStart == 6 && selections.back().selectionEnd == 6 &&
            selections.back().eventCount == 1, "unchanged acknowledgment swallowed native selection");
    require(!bridge->applyJS(0, QStringLiteral("stale"), 0, 0), "stale token accepted");
    require(input->property("text").toString() == QStringLiteral("A😀e\u0301X"), "stale value replaced native text");
    require(bridge->applyJS(1, std::nullopt, 1, 3), "current selection rejected");
    require(input->property("selectedText").toString() == QStringLiteral("😀"), "selection was not UTF-16");
    require(changes.size() == 1 && bridge->eventCount() == 1, "selection advanced native edit count");
    require(bridge->applyJS(1, QStringLiteral("bad"), 9, 10), "matching text was rejected for invalid selection");
    require(input->property("text").toString() == QStringLiteral("bad") && bridge->eventCount() == 1 && changes.size() == 1,
            "invalid selection changed RN text-first/no-echo contract");
    require(bridge->applyJS(1, QStringLiteral("A😀e\u0301X")), "restore failed");
    require(!bridge->applyJS(2, QStringLiteral("future")), "future token accepted");
    require(bridge->configure(true, false), "secure single-line unavailable");
    require(bridge->applyJS(1, QStringLiteral("synthetic-private-fixture")), "secure JS write rejected");
    QInputMethodQueryEvent query(Qt::ImEnabled | Qt::ImSurroundingText | Qt::ImCurrentSelection |
                                Qt::ImTextBeforeCursor | Qt::ImTextAfterCursor);
    QCoreApplication::sendEvent(input, &query);
    require(!query.value(Qt::ImEnabled).toBool(), "secure IME not explicitly disabled");
    for (auto type : {Qt::ImSurroundingText, Qt::ImCurrentSelection, Qt::ImTextBeforeCursor, Qt::ImTextAfterCursor}) {
      require(query.value(type).toString().isEmpty(), "secure query exposed text");
    }
    QInputMethodEvent forbidden;
    forbidden.setCommitString(QStringLiteral("forbidden"));
    QCoreApplication::sendEvent(input, &forbidden);
    require(input->property("text").toString() == QStringLiteral("synthetic-private-fixture") &&
            bridge->eventCount() == 1, "secure IME commit accepted");
    require(!bridge->configure(true, true) && !wrapper->isVisible(), "unsupported secure multiline stayed visible");
    auto* multiline = wrapper->findChild<QQuickItem*>("qnx-text-edit");
    QInputMethodQueryEvent hiddenQuery(Qt::ImEnabled | Qt::ImSurroundingText);
    QCoreApplication::sendEvent(multiline, &hiddenQuery);
    require(!hiddenQuery.value(Qt::ImEnabled).toBool() && hiddenQuery.value(Qt::ImSurroundingText).toString().isEmpty(),
            "inactive secure child exposed provider text");
    const auto generation = bridge->generation();
    QPointer<QtTextInputBridge> previous(bridge);
    bridge->invalidate();
    QInputMethodQueryEvent invalidatedQuery(Qt::ImEnabled | Qt::ImSurroundingText);
    QCoreApplication::sendEvent(input, &invalidatedQuery);
    require(!invalidatedQuery.value(Qt::ImEnabled).toBool() && invalidatedQuery.value(Qt::ImSurroundingText).toString().isEmpty(),
            "invalidated editor exposed provider text during detach");
    QMetaObject::invokeMethod(wrapper, "keyPressed");
    object.reset();
    require(previous.isNull(), "bridge survived wrapper deletion");
    std::unique_ptr<QObject> replacement(component.create());
    auto* replacementItem = qobject_cast<QQuickItem*>(replacement.get());
    QtTextInputBridge* next = new QtTextInputBridge(*replacementItem);
    require(next->generation() != generation && next->eventCount() == 0, "replacement editing generation reused");
    std::cout << "RNQNX_TEXT_INPUT_BRIDGE_OK count stale utf16 no-echo secure-query denial lifecycle\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_TEXT_INPUT_BRIDGE_FAIL " << error.what() << '\n';
    return 1;
  }
}
