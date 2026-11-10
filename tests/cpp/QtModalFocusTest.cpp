// Native host behavior only. The production consumer and physical HID are
// separate acceptance gates; this fixture does not install a second renderer.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>

#include <iostream>
#include <limits>
#include <stdexcept>

#if !RNQNX_ENABLE_MODAL_FOCUS
#error QtModalFocusTest requires RNQNX_ENABLE_MODAL_FOCUS
#endif

namespace {
using namespace rnqnx::fabric;

void expect(bool condition, const char* detail) {
  if (!condition) throw std::runtime_error(detail);
}

bool inside(QQuickItem* item, QQuickItem* modal) {
  for (; item; item = item->parentItem()) if (item == modal) return true;
  return false;
}

void key(QObject& target, QEvent::Type type, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
         bool repeat = false, const QString& text = {}) {
  QKeyEvent event(type, code, modifiers, text, repeat);
  QCoreApplication::sendEvent(&target, &event);
}

struct Fixture {
  QQmlEngine engine;
  QQuickWindow window;
  rnqnx::views::QtQuickItemFactory factory{engine};
  QtQuickMountingLayer layer{factory, *window.contentItem(), 1};

  Fixture() {
    factory.registerType("Modal", "import QtQuick\nItem {}");
    factory.registerType("Field", "import QtQuick\nTextInput { activeFocusOnTab: true }");
    window.showFullScreen();
    window.requestActivate();
    QCoreApplication::processEvents();
    add(2, "Field", 1, 0);
    layer.itemForTag(2)->forceActiveFocus();
  }

  QQuickItem* add(Tag tag, const char* type, Tag parent, int index, bool modal = false) {
    expect(layer.create(tag, QLatin1String(type), modal) == MountError::None, "create");
    expect(layer.insert(parent, tag, index) == MountError::None, "insert");
    return layer.itemForTag(tag);
  }

  void remove(Tag parent, Tag tag, int index) {
    expect(layer.remove(parent, tag, index) == MountError::None, "remove");
  }
};

void isolatesKeysAndTab() {
  Fixture f;
  auto* background = f.layer.itemForTag(2);
  auto* modal = f.add(3, "Modal", 1, 1, true);
  auto* first = f.add(4, "Field", 3, 0);
  auto* second = f.add(5, "Field", 3, 1);
  first->forceActiveFocus();
  key(f.window, QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, false, "x");
  expect(first->property("text").toString() == "x", "modal receives typing");
  key(*background, QEvent::KeyPress, Qt::Key_Y, Qt::NoModifier, false, "y");
  QInputMethodEvent composition;
  composition.setCommitString("z");
  QCoreApplication::sendEvent(background, &composition);
  expect(background->property("text").toString().isEmpty(), "background keys and IME blocked");
  for (int i = 0; i < 8; ++i) {
    key(f.window, QEvent::KeyPress, Qt::Key_Tab);
    expect(inside(f.window.activeFocusItem(), modal), "Tab remains modal");
    expect(f.window.activeFocusItem() == (i % 2 == 0 ? second : first), "Tab traverses native fields");
  }
  key(f.window, QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
  expect(f.window.activeFocusItem() == second, "Backtab wraps");
  background->forceActiveFocus();
  expect(inside(f.window.activeFocusItem(), modal), "programmatic background focus rejected");
  f.remove(1, 3, 1);
  expect(f.window.activeFocusItem() == background, "valid original focus restored");
}

void nestedEscapeAndCleanup() {
  Fixture f;
  auto* outer = f.add(3, "Modal", 1, 1, true);
  auto* outerField = f.add(4, "Field", 3, 0);
  outerField->forceActiveFocus();
  f.add(5, "Modal", 3, 1, true);
  auto* innerField = f.add(6, "Field", 5, 0);
  innerField->forceActiveFocus();
  int outerRequests = 0;
  int innerRequests = 0;
  f.layer.setModalRequestClose(3, [&] { ++outerRequests; });
  f.layer.setModalRequestClose(5, [&] { ++innerRequests; f.remove(3, 5, 1); });
  key(f.window, QEvent::KeyPress, Qt::Key_Escape);
  expect(innerRequests == 1 && outerRequests == 0, "only top Modal requests closure");
  expect(f.window.activeFocusItem() == outerField, "nested close restores outer field");
  key(f.window, QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier, true);
  key(f.window, QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier, true);
  key(f.window, QEvent::KeyPress, Qt::Key_Escape);
  expect(outerRequests == 0, "held Escape cannot dismiss exposed Modal");
  key(f.window, QEvent::KeyRelease, Qt::Key_Escape);
  key(f.window, QEvent::KeyPress, Qt::Key_Escape);
  expect(outerRequests == 1 && inside(f.window.activeFocusItem(), outer), "request does not hide controlled Modal");
  key(f.window, QEvent::KeyRelease, Qt::Key_Escape);
  // Reinsert a nested portal, then detach its logical ancestor first.
  expect(f.layer.insert(3, 5, 1) == MountError::None, "reopen nested");
  innerField->forceActiveFocus();
  outer->setVisible(false);
  expect(f.window.activeFocusItem() == f.layer.itemForTag(2), "hidden logical ancestor releases nested portal focus");
  outer->setVisible(true);
  expect(inside(f.window.activeFocusItem(), f.layer.itemForTag(5)), "showing ancestor restores top nested Modal");
  f.remove(1, 3, 1);
  expect(f.window.activeFocusItem() == f.layer.itemForTag(2), "ancestor removal clears nested ownership");
  expect(f.layer.destroy(3) == MountError::None, "destroy ancestor");
  expect(f.layer.destroy(5) == MountError::None, "destroy nested");
}

void rejectsInvalidRestoration() {
  for (int variant = 0; variant < 3; ++variant) {
    Fixture f;
    auto* background = f.layer.itemForTag(2);
    f.add(3, "Modal", 1, 1, true);
    if (variant == 0) background->setEnabled(false);
    if (variant == 1) background->setVisible(false);
    if (variant == 2) expect(f.layer.destroy(2) == MountError::None, "delete background");
    f.remove(1, 3, variant == 2 ? 0 : 1);
    expect(f.window.activeFocusItem() == f.window.contentItem(), "invalid focus falls back to window content");
    auto* reopened = f.add(7, "Modal", 1, variant == 2 ? 0 : 1, true);
    expect(inside(f.window.activeFocusItem(), reopened), "reopening acquires focus");
    reopened->setVisible(false);
    expect(!inside(f.window.activeFocusItem(), reopened),
           variant == 0 ? "hidden Modal releases focus (disabled background)"
           : variant == 1 ? "hidden Modal releases focus (hidden background)"
                          : "hidden Modal releases focus (deleted background)");
    reopened->setVisible(true);
    expect(inside(f.window.activeFocusItem(), reopened), "visible Modal reacquires focus");
  }
}

// Real platform activation: a synthetic WindowActivate does not give the
// window keyboard focus, so it cannot prove focus is retained.
void activate(QQuickWindow& window) {
  window.requestActivate();
  for (int i = 0; i < 200 && QGuiApplication::focusWindow() != &window; ++i) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
  expect(QGuiApplication::focusWindow() == &window, "platform activated window");
}

void windowOwnershipAndDeactivation() {
  Fixture first;
  Fixture second;
  first.add(3, "Modal", 1, 1, true);
  second.add(3, "Modal", 1, 1, true);
  int a = 0;
  int b = 0;
  first.layer.setModalRequestClose(3, [&] { ++a; });
  second.layer.setModalRequestClose(3, [&] { ++b; });
  activate(first.window);
  key(first.window, QEvent::KeyPress, Qt::Key_Escape);
  key(first.window, QEvent::KeyRelease, Qt::Key_Escape);
  expect(a == 1 && b == 0, "separate windows have separate top owners");
  activate(second.window);
  key(first.window, QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, false, "a");
  expect(first.layer.itemForTag(2)->property("text").toString().isEmpty(), "deactivation never admits background typing");
  activate(first.window);
  expect(inside(first.window.activeFocusItem(), first.layer.itemForTag(3)), "reactivation retains modality");
}

void sharesOwnershipAcrossSurfaces() {
  Fixture f;
  f.add(3, "Modal", 1, 1, true);
  int first = 0;
  int second = 0;
  f.layer.setModalRequestClose(3, [&] { ++first; });
  QQuickItem secondRoot(f.window.contentItem());
  secondRoot.setZ(std::numeric_limits<qreal>::max());
  QtQuickMountingLayer other(f.factory, secondRoot, 100);
  expect(other.create(101, "Modal", true) == MountError::None, "second surface create");
  expect(other.insert(100, 101, 0) == MountError::None, "second surface insert");
  other.setModalRequestClose(101, [&] { ++second; });
  key(f.window, QEvent::KeyPress, Qt::Key_Escape);
  key(f.window, QEvent::KeyRelease, Qt::Key_Escape);
  expect(first == 0 && second == 1, "one top owner across Fabric surfaces in a window");
  secondRoot.setZ(-1);
  key(f.window, QEvent::KeyPress, Qt::Key_Escape);
  key(f.window, QEvent::KeyRelease, Qt::Key_Escape);
  expect(first == 1 && second == 1, "visual stack, not last registered surface, owns Escape");
  expect(other.remove(100, 101, 0) == MountError::None, "remove second surface Modal");
  expect(inside(f.window.activeFocusItem(), f.layer.itemForTag(3)), "closing second surface returns to first Modal");
}
} // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  try {
    isolatesKeysAndTab();
    nestedEscapeAndCleanup();
    rejectsInvalidRestoration();
    windowOwnershipAndDeactivation();
    sharesOwnershipAcrossSurfaces();
    std::cout << "RNQNX_MODAL_FOCUS_OK\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
