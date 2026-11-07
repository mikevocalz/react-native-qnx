#include "react-native-qnx/fabric/QtTextInputBridge.h"
#include "react-native-qnx/views/QtTextInputQml.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QQuickItem>
#include <QPointer>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace rnqnx::fabric;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(rnqnx::views::textInputQml(), QUrl());
    require(component.isReady(), "TextInput QML unavailable");
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
