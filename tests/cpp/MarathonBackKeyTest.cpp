#include "MarathonBackKey.hpp"

#include <QGuiApplication>
#include <QKeyEvent>

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void expect(bool condition, const char* what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL %s\n", what);
    ++failures;
  }
}

// Returns true when the filter consumed the event.
bool send(QObject& target, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers = {},
          bool autoRepeat = false) {
  QKeyEvent event(type, key, modifiers, QString(), autoRepeat);
  return QCoreApplication::sendEvent(&target, &event) && event.isAccepted() ? true : false;
}
}  // namespace

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  QObject target;
  int presses = 0;
  rnqnx::marathon::BackKeyFilter filter([&presses] { ++presses; });
  target.installEventFilter(&filter);

  // eventFilter's return value is the contract, so call it directly as well.
  QKeyEvent back(QEvent::KeyPress, Qt::Key_Back, Qt::NoModifier);
  expect(filter.eventFilter(&target, &back), "Key_Back press is consumed");
  expect(presses == 1, "Key_Back press calls the handler once");

  QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Back, Qt::NoModifier, QString(), true);
  expect(filter.eventFilter(&target, &repeat), "auto-repeat Key_Back is consumed");
  expect(presses == 1, "auto-repeat does not call the handler again");

  QKeyEvent release(QEvent::KeyRelease, Qt::Key_Back, Qt::NoModifier);
  expect(!filter.eventFilter(&target, &release), "Key_Back release is not consumed");
  expect(presses == 1, "release does not call the handler");

  // The shell's fallback keys are ordinary keys for the focused item.
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QKeyEvent altLeft(QEvent::KeyPress, Qt::Key_Left, Qt::AltModifier);
  QKeyEvent letter(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  expect(!filter.eventFilter(&target, &escape), "Escape passes through");
  expect(!filter.eventFilter(&target, &altLeft), "Alt+Left passes through");
  expect(!filter.eventFilter(&target, &letter), "other keys pass through");
  expect(presses == 1, "fallback and ordinary keys never call the handler");

  // Through the real event delivery path too.
  const bool delivered = send(target, QEvent::KeyPress, Qt::Key_Back);
  (void)delivered;
  expect(presses == 2, "sendEvent reaches the filter and the handler");

  if (failures != 0) return EXIT_FAILURE;
  std::puts("MarathonBackKeyTest passed");
  return EXIT_SUCCESS;
}
