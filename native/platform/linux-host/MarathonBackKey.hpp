#pragma once

// Marathon-Shell delivers Back to a native Wayland window as the XF86Back key
// (docs/NATIVE_APP_LIFECYCLE.md, "What Marathon sends"; shell/src/wayland/
// waylandcompositor.cpp sendNativeBack). Qt maps that keysym to Qt::Key_Back.
// This filter turns it into one callback per press and consumes it.
//
// Escape and Alt+Left are not touched: the shell sends them only as a fallback
// 180 ms after an XF86Back that changed nothing visible, and the focused item
// should receive them as ordinary keys.

#include <QEvent>
#include <QKeyEvent>
#include <QObject>

#include <functional>
#include <utility>

namespace rnqnx::marathon {

class BackKeyFilter final : public QObject {
 public:
  using Handler = std::function<void()>;

  explicit BackKeyFilter(Handler handler, QObject* parent = nullptr)
      : QObject(parent), handler_(std::move(handler)) {}

  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() != QEvent::KeyPress) return QObject::eventFilter(watched, event);
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() != Qt::Key_Back) return QObject::eventFilter(watched, event);
    // Auto-repeat is part of one physical press: deliver once, consume all.
    if (!key->isAutoRepeat() && handler_) handler_();
    return true;
  }

 private:
  Handler handler_;
};

}  // namespace rnqnx::marathon
