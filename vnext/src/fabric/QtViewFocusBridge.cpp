#include "react-native-qnx/fabric/QtViewFocusBridge.h"

#include <QKeyEvent>

#include <utility>

namespace rnqnx::fabric {

QtViewFocusBridge::QtViewFocusBridge(QQuickItem& item)
    : QObject(&item), item_(item) {}

void QtViewFocusBridge::update(
    Changed changed, std::function<void()> activate, bool tabFocusable) {
  changed_ = std::move(changed);
  activate_ = std::move(activate);
  tabFocusable_ = tabFocusable;
  if (!tabFocusable_) pressedKey_.reset();
  applyTabPolicy();
}

void QtViewFocusBridge::applyTabPolicy() {
  const bool eligible = mounted_ && tabFocusable_;
  if (item_.activeFocusOnTab() == eligible) return;
  // Qt refuses to remove the current active item from its tab sequence. A
  // button that becomes disabled/detached must relinquish focus first. Avoid
  // touching an unchanged false policy on a programmatically focused plain View.
  if (!eligible && item_.hasActiveFocus()) item_.setFocus(false);
  item_.setActiveFocusOnTab(eligible);
}

void QtViewFocusBridge::setMounted(bool mounted) {
  if (mounted_ == mounted) return;
  QObject::disconnect(connection_);
  connection_ = {};
  mounted_ = mounted;
  pressedKey_.reset();
  applyTabPolicy();
  if (!mounted_) {
    item_.removeEventFilter(this);
    return;
  }

  item_.installEventFilter(this);
  connection_ = QObject::connect(
      &item_, &QQuickItem::activeFocusChanged, this, [this](bool focused) {
        if (!focused) pressedKey_.reset();
        const auto changed = changed_;
        if (changed) changed(focused);
      });
  // A reinserted item can already hold focus when it enters the window. Its
  // current focus belongs to this mount, after the previous mount disconnected.
  if (item_.hasActiveFocus()) {
    const auto changed = changed_;
    if (changed) changed(true);
  }
}

bool QtViewFocusBridge::eventFilter(QObject* watched, QEvent* event) {
  if (watched != &item_ ||
      (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)) {
    return false;
  }
  const auto* key = static_cast<QKeyEvent*>(event);
  if (!mounted_ || !tabFocusable_ || !item_.hasActiveFocus() ||
      !item_.isEnabled() || !item_.isVisible() ||
      key->modifiers() != Qt::NoModifier) {
    pressedKey_.reset();
    return false;
  }
  if (key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter && key->key() != Qt::Key_Space) {
    return false;
  }
  event->accept();
  // Qt can deliver repeated press/release pairs while a key is held. Preserve
  // the original arm until its non-repeated release, then activate exactly once.
  if (key->isAutoRepeat()) return true;
  if (event->type() == QEvent::KeyPress) {
    if (!pressedKey_) pressedKey_ = key->key();
    return true;
  }
  const bool activate = pressedKey_ == key->key();
  pressedKey_.reset();
  const auto callback = activate_;
  if (activate && callback) callback();
  return true;
}

}  // namespace rnqnx::fabric
