#include "react-native-qnx/fabric/QtDesktopInput.h"
#include <react/renderer/components/view/ViewEventEmitter.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/mounting/ShadowView.h>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <algorithm>
#include <functional>

namespace rnqnx::fabric {
using facebook::react::PointerEventsMode;

QtDesktopInput::QtDesktopInput(QtQuickMountingLayer& layer, QQuickItem& root)
    : layer_(layer), root_(root) {}

QtDesktopInput::~QtDesktopInput() {
  for (const auto& entry : entries_) QObject::disconnect(entry.focusConnection);
}

void QtDesktopInput::setView(const facebook::react::ShadowView& view) {
  auto* item = layer_.itemForTag(view.tag);
  const auto* props = dynamic_cast<const facebook::react::ViewProps*>(view.props.get());
  const auto emitter = std::dynamic_pointer_cast<const facebook::react::TouchEventEmitter>(view.eventEmitter);
  if (!item || !props || !emitter) { forgetTag(view.tag); return; }
  const QString name = QString::fromUtf8(view.componentName ? view.componentName : "");
  const bool editable = name.contains(QStringLiteral("TextInput"));
  auto found = entries_.find(view.tag);
  if (found == entries_.end()) {
    Entry entry;
    entry.item = item;
    if (!editable) {
      entry.focusConnection = QObject::connect(item, &QQuickItem::activeFocusChanged, this, [this, tag = view.tag](bool) {
        const auto current = entries_.value(tag);
        if (!current.item || (current.disabled && current.item->hasActiveFocus())) return;
        if (auto viewEmitter = std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(current.emitter)) {
          if (current.item->hasActiveFocus()) viewEmitter->onFocus();
          else viewEmitter->onBlur();
        }
        if (!current.item->hasActiveFocus() && keyPressTag_ == tag) keyPressTag_ = -1;
      });
    }
    found = entries_.insert(view.tag, entry);
  }
  found->emitter = emitter;
  found->editable = editable;
  found->modal = name == QStringLiteral("ModalHostView");
  found->disabled = props->accessibilityState && props->accessibilityState->disabled;
  found->focusable = props->focusable && !found->disabled;
  found->keyHandler = props->onKeyDown || props->onKeyDownCapture || props->onKeyUp || props->onKeyUpCapture;
  found->pointerEvents = props->pointerEvents;
  item->setProperty("rnqnxTestId", QString::fromStdString(props->testId));
  if (name == QStringLiteral("View") || name == QStringLiteral("SafeAreaView") || found->modal)
    item->setClip(props->getClipsContentToBounds());
  if (!editable) {
    item->setEnabled(!found->disabled);
    if (item->activeFocusOnTab() != found->focusable) {
      // Qt refuses to remove a tab stop while it still owns active focus.
      if (!found->focusable && item->hasActiveFocus()) item->setFocus(false);
      item->setActiveFocusOnTab(found->focusable);
    }
  }
  if (found->disabled && item->hasActiveFocus()) item->setFocus(false);
}

void QtDesktopInput::forgetTag(Tag tag) {
  const auto entry = entries_.take(tag);
  QObject::disconnect(entry.focusConnection);
  hovered_.erase(std::remove(hovered_.begin(), hovered_.end(), tag), hovered_.end());
  if (keyPressTag_ == tag) keyPressTag_ = -1;
}

Tag QtDesktopInput::tagForItem(const QQuickItem* item) const {
  for (auto it = entries_.cbegin(); it != entries_.cend(); ++it)
    if (it->item == item) return it.key();
  return -1;
}

QQuickItem* QtDesktopInput::modalRoot() const {
  std::function<QQuickItem*(QQuickItem*)> visit = [&](QQuickItem* item) -> QQuickItem* {
    if (!item->isVisible()) return nullptr;
    auto children = item->childItems();
    std::stable_sort(children.begin(), children.end(), [](auto* left, auto* right) { return left->z() < right->z(); });
    for (auto it = children.crbegin(); it != children.crend(); ++it) {
      if (auto* modal = visit(*it)) return modal;
    }
    return entries_.value(tagForItem(item)).modal ? item : nullptr;
  };
  return visit(&root_);
}

bool QtDesktopInput::inModalScope(const QQuickItem* item) const {
  const auto* modal = modalRoot();
  return !modal || modal == item || modal->isAncestorOf(item);
}

Tag QtDesktopInput::hitTest(const QPointF& point) const {
  std::function<Tag(const QQuickItem*)> visit = [&](const QQuickItem* item) -> Tag {
    if (!item->isVisible() || !item->isEnabled()) return -1;
    const auto tag = tagForItem(item);
    const auto entry = entries_.value(tag);
    if (entry.pointerEvents == PointerEventsMode::None) return -1;
    const bool inside = item->contains(item->mapFromScene(point));
    if (!inside && item->clip()) return -1;
    if (entry.pointerEvents != PointerEventsMode::BoxOnly) {
      auto children = item->childItems();
      std::stable_sort(children.begin(), children.end(), [](auto* left, auto* right) { return left->z() < right->z(); });
      for (auto it = children.crbegin(); it != children.crend(); ++it) {
        const Tag child = visit(*it);
        if (child != -1) return child;
      }
    }
    return inside && tag != -1 && entry.pointerEvents != PointerEventsMode::BoxNone ? tag : -1;
  };
  const auto* modal = modalRoot();
  return visit(modal ? modal : &root_);
}

std::vector<Tag> QtDesktopInput::ancestry(Tag tag) const {
  std::vector<Tag> result;
  const auto entry = entries_.value(tag);
  for (auto* item = entry.item.data(); item && item != &root_; item = item->parentItem()) {
    const Tag parent = tagForItem(item);
    if (parent != -1) result.push_back(parent);
  }
  return result;
}

Tag QtDesktopInput::focusTarget() const {
  const auto* window = root_.window();
  for (auto* item = window ? window->activeFocusItem() : nullptr; item && item != &root_; item = item->parentItem()) {
    const Tag tag = tagForItem(item);
    if (tag != -1 && inModalScope(item)) return tag;
  }
  return -1;
}

void QtDesktopInput::hover(const QMouseEvent* event) {
  if (event) lastPoint_ = event->position();
  const Tag target = event ? hitTest(lastPoint_) : -1;
  if (target != -1) {
    const auto entry = entries_.value(target);
    if (entry.item && entry.emitter) {
      // RN's PointerEventsProcessor owns hover ancestry and emits enter,
      // leave, over and out. Sending those per ancestor here would duplicate
      // transitions and interpret each leave as an exit from the whole root.
      entry.emitter->onPointerMove(desktop::pointerEvent(event, *entry.item, lastPoint_));
      hovered_ = ancestry(target);
    }
    return;
  }
  for (const Tag tag : hovered_) {
    const auto entry = entries_.value(tag);
    if (entry.item && entry.emitter) {
      entry.emitter->onPointerLeave(desktop::pointerEvent(event, *entry.item, lastPoint_));
      break;
    }
  }
  hovered_.clear();
}

void QtDesktopInput::moveFocus(bool forward) {
  auto* window = root_.window();
  auto* start = window ? window->activeFocusItem() : nullptr;
  if (!start || !root_.isAncestorOf(start)) start = &root_;
  auto* next = start;
  for (int count = 0; count <= entries_.size() * 4 + 16; ++count) {
    next = next->nextItemInFocusChain(forward);
    if (!next || next == start) return;
    const auto entry = entries_.value(tagForItem(next));
    if (entry.item && (entry.focusable || entry.editable) && !entry.disabled &&
        next->isVisible() && next->isEnabled() && inModalScope(next)) {
      next->forceActiveFocus(forward ? Qt::TabFocusReason : Qt::BacktabFocusReason);
      return;
    }
  }
}

bool QtDesktopInput::key(QKeyEvent& event) {
  if (event.key() == Qt::Key_Back) return false;
  const Tag tag = focusTarget();
  const auto entry = entries_.value(tag);
  bool handler = false;
  for (auto parent : ancestry(tag)) handler = handler || entries_.value(parent).keyHandler;
  if (entry.emitter && !entry.disabled) desktop::keyEvent(*entry.emitter, tag, event);
  if (event.key() == Qt::Key_Tab || event.key() == Qt::Key_Backtab) {
    if (event.type() == QEvent::KeyPress) moveFocus(event.key() != Qt::Key_Backtab && !event.modifiers().testFlag(Qt::ShiftModifier));
    return true;
  }
  const bool activation = event.key() == Qt::Key_Return || event.key() == Qt::Key_Enter || event.key() == Qt::Key_Space;
  const bool plain = !(event.modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
  if (!entry.emitter || entry.disabled || entry.editable) return false;
  if (activation && plain && entry.focusable && !entry.keyHandler) {
    if (event.type() == QEvent::KeyPress && !event.isAutoRepeat()) {
      keyPressTag_ = tag; keyPressCode_ = event.key();
    } else if (event.type() == QEvent::KeyRelease && !event.isAutoRepeat()) {
      const bool activate = keyPressTag_ == tag && keyPressCode_ == event.key();
      keyPressTag_ = -1;
      if (activate) desktop::keyboardClick(*entry.emitter, tag, event);
    }
    return true;
  }
  if (handler) keyPressTag_ = -1;
  return handler;
}

bool QtDesktopInput::handleEvent(QObject* watched, QEvent* event) {
  if (watched != root_.window()) return false;
  switch (event->type()) {
    case QEvent::MouseMove: hover(static_cast<QMouseEvent*>(event)); break;
    case QEvent::MouseButtonPress: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      hover(mouse);
      if (mouse->button() == Qt::LeftButton) for (auto tag : ancestry(hitTest(mouse->position()))) {
        const auto entry = entries_.value(tag);
        if (entry.item && entry.focusable && !entry.disabled && !entry.editable) {
          entry.item->forceActiveFocus(Qt::MouseFocusReason); break;
        }
      }
      break;
    }
    case QEvent::Leave: hover(nullptr); break;
    case QEvent::WindowDeactivate: hover(nullptr); keyPressTag_ = -1; break;
    case QEvent::KeyPress: case QEvent::KeyRelease: return key(*static_cast<QKeyEvent*>(event));
    default: break;
  }
  return false;
}
}  // namespace rnqnx::fabric
