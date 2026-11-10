#include "react-native-qnx/fabric/QtQuickMountingLayer.h"

#include "react-native-qnx/views/QtQuickItemFactory.h"
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
#include "react-native-qnx/fabric/QtTextInputBridge.h"
#endif

#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
#include "react-native-qnx/fabric/QtViewAccessibility.h"
#endif
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QHash>
#include <QMouseEvent>
#include <QStringList>
#include <QUrl>
#include <QTouchEvent>
#include <QWheelEvent>
#if RNQNX_ENABLE_MODAL_FOCUS
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QScopedValueRollback>
#include <QSet>
#include <QTimer>
#endif

#include <algorithm>
#include <limits>
#include <QList>
#include <QQuickItem>

namespace {

// React's responder tree and Qt's native item delivery are separate paths.
// A nonpainting Modal would otherwise let Qt wheel/MouseArea events fall
// through to the application behind it. This lowest child is a fallback;
// native controls mounted above it inside the Modal still receive input first.
class ModalPointerBarrier final : public QQuickItem {
 public:
  explicit ModalPointerBarrier(QQuickItem& modal) : QQuickItem(&modal) {
    setZ(std::numeric_limits<qreal>::lowest());
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptTouchEvents(true);
    const auto resize = [this, &modal] { setSize(modal.size()); };
    QObject::connect(&modal, &QQuickItem::widthChanged, this, resize);
    QObject::connect(&modal, &QQuickItem::heightChanged, this, resize);
    resize();
  }

 protected:
  void mousePressEvent(QMouseEvent* event) override { event->accept(); }
  void mouseMoveEvent(QMouseEvent* event) override { event->accept(); }
  void mouseReleaseEvent(QMouseEvent* event) override { event->accept(); }
  void mouseDoubleClickEvent(QMouseEvent* event) override { event->accept(); }
  void touchEvent(QTouchEvent* event) override { event->accept(); }
  void wheelEvent(QWheelEvent* event) override { event->accept(); }
};

#if RNQNX_ENABLE_MODAL_FOCUS
// One owner per native window, including surfaces with different Fabric roots.
class ModalFocusOwner final : public QObject {
 public:
  explicit ModalFocusOwner(QQuickWindow& window) : QObject(&window), window_(window) {
    QCoreApplication::instance()->installEventFilter(this);
    QObject::connect(&window_, &QQuickWindow::activeFocusItemChanged, this, [this] { refresh(); });
    QObject::connect(&window_, &QObject::destroyed, this, [this] { suspended_ = true; });
  }

  static ModalFocusOwner* forWindow(QQuickWindow& window) {
    for (auto* child : window.children()) {
      if (auto* owner = dynamic_cast<ModalFocusOwner*>(child)) return owner;
    }
    return new ModalFocusOwner(window);
  }

  void add(QQuickItem& item, std::function<void()> requestClose,
           QList<QPointer<QQuickItem>> ancestors) {
    if (entries_.contains(&item)) {
      entries_[&item].requestClose = std::move(requestClose);
      refresh();
      queueRefresh();
      return;
    }
    entries_.insert(&item, Entry{&item, {}, std::move(requestClose), ancestors});
    order_.append(&item);
    auto& connections = connections_[&item];
    connections.append(QObject::connect(&item, &QQuickItem::visibleChanged, this, [this] { refresh(); }));
    connections.append(QObject::connect(&item, &QQuickItem::enabledChanged, this, [this] { refresh(); }));
    connections.append(QObject::connect(&item, &QObject::destroyed, this, [this, key = &item] { remove(key); }));
    for (const auto& ancestor : ancestors) {
      if (!ancestor) continue;
      connections.append(QObject::connect(ancestor, &QQuickItem::visibleChanged, this, [this] { refresh(); }));
      connections.append(QObject::connect(ancestor, &QQuickItem::enabledChanged, this, [this] { refresh(); }));
      connections.append(QObject::connect(ancestor, &QQuickItem::windowChanged, this, [this] { refresh(); }));
      connections.append(QObject::connect(ancestor, &QObject::destroyed, this, [this] { refresh(); }));
    }
    refresh();
    queueRefresh();
  }

  void remove(QQuickItem* key, bool refreshNow = true) {
    for (const auto& connection : connections_.take(key)) QObject::disconnect(connection);
    entries_.remove(key);
    order_.removeOne(key);
    if (refreshNow) refresh();
  }

  void settle() { refresh(); }

 private:
  struct Entry {
    QPointer<QQuickItem> item;
    QPointer<QQuickItem> lastFocus;
    std::function<void()> requestClose;
    QList<QPointer<QQuickItem>> ancestors;
  };

  static bool inside(const QQuickItem* item, const QQuickItem* modal) {
    for (; item; item = item->parentItem()) if (item == modal) return true;
    return false;
  }

  bool usable(const QQuickItem* item) const {
    return item && item->window() == &window_ && item->isVisible() && item->isEnabled();
  }

  QQuickItem* top() const {
    QQuickItem* result = nullptr;
    for (auto it = order_.crbegin(); it != order_.crend(); ++it) {
      const auto& entry = entries_[*it];
      const auto item = entry.item;
      const bool ancestorsUsable = std::all_of(entry.ancestors.cbegin(), entry.ancestors.cend(),
          [this](const auto& ancestor) { return usable(ancestor); });
      if (usable(item) && ancestorsUsable && (!result || above(item, result))) result = item;
    }
    return result;
  }

  static bool above(QQuickItem* item, QQuickItem* other) {
    QList<QQuickItem*> path;
    QList<QQuickItem*> otherPath;
    for (auto* current = item; current; current = current->parentItem()) path.prepend(current);
    for (auto* current = other; current; current = current->parentItem()) otherPath.prepend(current);
    while (!path.isEmpty() && !otherPath.isEmpty() && path.first() == otherPath.first()) {
      path.removeFirst();
      otherPath.removeFirst();
    }
    if (path.isEmpty()) return false;
    if (otherPath.isEmpty()) return true;
    auto* branch = path.first();
    auto* otherBranch = otherPath.first();
    if (branch->z() != otherBranch->z()) return branch->z() > otherBranch->z();
    auto* parent = branch->parentItem();
    if (!parent || parent != otherBranch->parentItem()) return false;
    const auto siblings = parent->childItems();
    return siblings.indexOf(branch) > siblings.indexOf(otherBranch);
  }

  QQuickItem* tabTarget(QQuickItem* modal, QQuickItem* start, bool forward) const {
    QSet<QQuickItem*> visited;
    auto* candidate = start ? start : modal;
    while (candidate && !visited.contains(candidate)) {
      visited.insert(candidate);
      candidate = candidate->nextItemInFocusChain(forward);
      if (candidate && inside(candidate, modal) && usable(candidate) && candidate->activeFocusOnTab()) return candidate;
    }
    return modal;
  }

  void queueRefresh() {
    if (refreshQueued_) return;
    refreshQueued_ = true;
    // Fabric inserts the Modal before its controls, and a View becomes tabbable
    // only after its insertion completes. Settle once after that mounting turn.
    // The window owns this receiver; its destruction cancels the queued call.
    const QPointer<ModalFocusOwner> owner(this);
    QTimer::singleShot(0, this, [owner] {
      if (!owner) return;
      owner->refreshQueued_ = false;
      owner->refresh();
    });
  }

  void refresh() {
    if (changing_ || suspended_ || !window_.contentItem()) return;
    QScopedValueRollback<bool> changing(changing_, true);
    auto* modal = top();
    auto* active = window_.activeFocusItem();
    if (modal == topKey_ && (!modal || (active != modal && inside(active, modal)))) {
      if (modal && usable(active)) entries_[modal].lastFocus = active;
      return;
    }
    if (!topKey_ && modal) backgroundFocus_ = active;
    if (entries_.contains(topKey_) && usable(active) && inside(active, topKey_)) {
      entries_[topKey_].lastFocus = active;
    }
    auto* previousTop = topKey_;
    topKey_ = modal;
    if (modal) {
      auto* candidate = entries_[modal].lastFocus.data();
      // The Modal itself is only the fallback while no eligible control exists.
      // Preserve explicitly focused descendants, including non-tab-focusable ones.
      if (candidate == modal || !usable(candidate) || !inside(candidate, modal)) candidate = tabTarget(modal, modal, true);
      candidate->forceActiveFocus(Qt::OtherFocusReason);
    } else {
      const auto previous = backgroundFocus_;
      backgroundFocus_.clear();
      // Forcing focus onto a scope hands it back to the scope's focused child,
      // so drop the closed Modal's focus first.
      if (active && previousTop && inside(active, previousTop)) active->setFocus(false, Qt::OtherFocusReason);
      // Detached, hidden, disabled or destroyed background items are never restored.
      (usable(previous) ? previous.data() : window_.contentItem())->forceActiveFocus(Qt::OtherFocusReason);
    }
  }

  bool eventFilter(QObject* watched, QEvent* event) override {
    auto* item = qobject_cast<QQuickItem*>(watched);
    if (watched != &window_ && (!item || item->window() != &window_)) return false;
    if (watched == &window_ && event->type() == QEvent::WindowDeactivate) {
      suspended_ = true;
      escapeDown_ = false;
      return false;
    }
    if (watched == &window_ && event->type() == QEvent::WindowActivate) {
      suspended_ = false;
      refresh();
      return false;
    }
    const bool keyEvent = event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease ||
        event->type() == QEvent::ShortcutOverride;
    if (!keyEvent && event->type() != QEvent::InputMethod && event->type() != QEvent::InputMethodQuery) return false;
    auto* modal = top();
    if (modal && suspended_) {
      event->accept();
      return true;
    }
    if (keyEvent) {
      auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier && (modal || escapeDown_)) {
        std::function<void()> requestClose;
        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat() && !escapeDown_) {
          escapeDown_ = true;
          if (modal) requestClose = entries_[modal].requestClose;
        } else if (event->type() == QEvent::KeyRelease && !key->isAutoRepeat()) {
          escapeDown_ = false;
        }
        event->accept();
        if (requestClose) requestClose();
        return true;
      }
      if (modal && (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) &&
          (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier)) {
        if (event->type() == QEvent::KeyPress) {
          const bool forward = key->key() == Qt::Key_Tab && !(key->modifiers() & Qt::ShiftModifier);
          tabTarget(modal, window_.activeFocusItem(), forward)->forceActiveFocus(
              forward ? Qt::TabFocusReason : Qt::BacktabFocusReason);
        }
        event->accept();
        return true;
      }
      if (modal && event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
      }
    }
    if (modal && (suspended_ || (item && !inside(item, modal)) || !inside(window_.activeFocusItem(), modal))) {
      refresh();
      event->accept();
      return true;
    }
    return false;
  }

  QQuickWindow& window_;
  QHash<QQuickItem*, Entry> entries_;
  QHash<QQuickItem*, QList<QMetaObject::Connection>> connections_;
  QList<QQuickItem*> order_;
  QQuickItem* topKey_{nullptr}; // Identity only; never dereferenced after removal.
  QPointer<QQuickItem> backgroundFocus_;
  bool refreshQueued_{false};
  bool changing_{false};
  bool suspended_{false};
  bool escapeDown_{false};
};
#endif

} // namespace

namespace rnqnx::fabric {

const char* describe(MountError error) {
  switch (error) {
    case MountError::None:
      return "None";
    case MountError::UnknownComponentType:
      return "UnknownComponentType";
    case MountError::DuplicateTag:
      return "DuplicateTag";
    case MountError::UnknownTag:
      return "UnknownTag";
    case MountError::UnknownParent:
      return "UnknownParent";
    case MountError::NotAChildOfParent:
      return "NotAChildOfParent";
    case MountError::IndexOutOfRange:
      return "IndexOutOfRange";
    case MountError::UnsupportedAccessibility:
      return "unsupported-accessibility";
    case MountError::UnsupportedTextInput:
      return "unsupported-text-input";
    case MountError::ItemCreationFailed:
      return "ItemCreationFailed";
  }
  return "Unknown";
}

QtQuickMountingLayer::QtQuickMountingLayer(
    views::QtQuickItemFactory& factory,
    QQuickItem& rootItem,
    Tag rootTag)
    : factory_(factory), rootItem_(rootItem), rootTag_(rootTag) {
  items_.insert(rootTag_, MountedItem{&rootItem_, false});
}

QtQuickMountingLayer::~QtQuickMountingLayer() {
#if RNQNX_ENABLE_MODAL_FOCUS
  tearingDown_ = true;
  for (auto it = modalFocusOwners_.cbegin(); it != modalFocusOwners_.cend(); ++it) {
    if (auto* owner = dynamic_cast<ModalFocusOwner*>(it.value().data())) owner->remove(itemForTag(it.key()), false);
  }
#endif
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  // Close every callback before changing any item's focus or deleting a tree.
  // A destructor must not admit reentrant user mutation of the owned registry.
  for (const auto& mounted : std::as_const(items_)) {
    if (mounted.owned) {
      if (auto* bridge = mounted.item->findChild<QtTextInputBridge*>(QString(), Qt::FindDirectChildrenOnly)) bridge->setEmitter({});
    }
  }
  for (const auto& mounted : std::as_const(items_)) {
    if (mounted.owned) {
      if (auto* bridge = mounted.item->findChild<QtTextInputBridge*>(QString(), Qt::FindDirectChildrenOnly)) bridge->invalidate();
    }
  }
#endif
#if RNQNX_ENABLE_MODAL_FOCUS
  for (const auto& guarded : std::as_const(modalFocusOwners_)) {
    if (auto* owner = dynamic_cast<ModalFocusOwner*>(guarded.data())) owner->settle();
  }
  modalFocusOwners_.clear();
#endif
  // Items are owned individually rather than through QObject parentage, so a
  // surviving tree is torn down one item at a time. QQuickItem's destructor
  // detaches any remaining visual children, so order does not matter.
  for (const auto& mounted : std::as_const(items_)) {
    if (mounted.owned) {
      delete mounted.item;
    }
  }
}

QString QtQuickMountingLayer::lastError() const {
  return lastError_;
}

int QtQuickMountingLayer::mountedCount() const {
  return static_cast<int>(items_.size());
}

QQuickItem* QtQuickMountingLayer::itemForTag(Tag tag) const {
  return items_.value(tag, MountedItem{}).item;
}

QQuickItem& QtQuickMountingLayer::rootItem() const {
  return rootItem_;
}

#if RNQNX_ENABLE_MODAL_FOCUS
void QtQuickMountingLayer::setModalRequestClose(Tag tag, std::function<void()> requestClose) {
  modalCloseRequests_.insert(tag, std::move(requestClose));
  syncModalFocus();
}

void QtQuickMountingLayer::syncModalFocus() {
  if (tearingDown_) return;
  // Remove first, so a detached nested portal cannot become the next owner.
  const auto rooted = [this](Tag tag) {
    QSet<Tag> visited;
    while (tag != rootTag_) {
      if (visited.contains(tag)) return false;
      visited.insert(tag);
      const auto mounted = items_.constFind(tag);
      if (mounted == items_.cend() || !mounted->parentTag) return false;
      tag = *mounted->parentTag;
    }
    return true;
  };
  QSet<ModalFocusOwner*> changed;
  for (const auto tag : modalFocusOwners_.keys()) {
    auto* item = itemForTag(tag);
    auto* owner = dynamic_cast<ModalFocusOwner*>(modalFocusOwners_.value(tag).data());
    if (!owner || !item || !rooted(tag) || owner->parent() != item->window()) {
      if (owner) {
        owner->remove(item, false);
        changed.insert(owner);
      }
      modalFocusOwners_.remove(tag);
    }
  }
  for (auto* owner : changed) owner->settle();
  for (const auto tag : modalOrder_) {
    auto* item = itemForTag(tag);
    if (!item || !rooted(tag) || !item->window()) continue;
    auto* owner = ModalFocusOwner::forWindow(*item->window());
    modalFocusOwners_.insert(tag, owner);
    QList<QPointer<QQuickItem>> ancestors;
    auto parent = items_[tag].parentTag;
    while (parent) {
      ancestors.append(itemForTag(*parent));
      parent = items_[*parent].parentTag;
    }
    owner->add(*item, modalCloseRequests_.value(tag), std::move(ancestors));
  }
}
#endif

#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
bool QtQuickMountingLayer::isGenericView(Tag tag) const {
  const auto mounted = items_.constFind(tag);
  return mounted != items_.constEnd() && mounted->owned && mounted->genericView;
}
#endif

MountError QtQuickMountingLayer::failWith(
    MountError error,
    const QString& detail) {
  lastError_ = detail;
  return error;
}

MountError QtQuickMountingLayer::create(
    Tag tag,
    const QString& componentName,
    bool viewportModal) {
  if (items_.contains(tag)) {
    return failWith(
        MountError::DuplicateTag,
        QStringLiteral("Tag %1 is already mounted").arg(tag));
  }

  QQuickItem* item = factory_.create(componentName, nullptr);
  if (item == nullptr) {
    const QString detail = factory_.lastError();
    return failWith(
        factory_.isCompiled(componentName) ? MountError::ItemCreationFailed
                                           : MountError::UnknownComponentType,
        detail);
  }

  MountedItem mounted{item, true};
  mounted.viewportModal = viewportModal;
  if (viewportModal) {
    item->setZ(std::numeric_limits<qreal>::max());
    new ModalPointerBarrier(*item);
#if RNQNX_ENABLE_MODAL_FOCUS
    QObject::connect(item, &QQuickItem::windowChanged, item, [this] { syncModalFocus(); });
#endif
  }
  mounted.genericView = componentName == QStringLiteral("View");
  items_.insert(tag, mounted);
  lastError_.clear();
  return MountError::None;
}

void QtQuickMountingLayer::raiseModals() {
  // Keep the visual order and the dispatcher's hit order above ordinary
  // surface children, including siblings inserted after a Modal was opened.
  for (const auto tag : modalOrder_) {
    auto* item = itemForTag(tag);
    const auto children = rootItem_.childItems();
    if (item != nullptr && !children.isEmpty() && children.last() != item) {
      item->stackAfter(children.last());
    }
  }
}

MountError QtQuickMountingLayer::insert(
    Tag parentTag,
    Tag childTag,
    int index) {
  const auto parent = items_.find(parentTag);
  if (parent == items_.end()) {
    return failWith(
        MountError::UnknownParent,
        QStringLiteral("Parent tag %1 is not mounted").arg(parentTag));
  }

  const auto child = items_.find(childTag);
  if (child == items_.end()) {
    return failWith(
        MountError::UnknownTag,
        QStringLiteral("Child tag %1 is not mounted").arg(childTag));
  }

  // Fabric indices include logical Modal children and exclude QML decoration.
  // They cannot be inferred from visual parentage once a Modal is presented.
  const int childCount = static_cast<int>(parent->children.size());
  if (index < 0 || index > childCount) {
    return failWith(
        MountError::IndexOutOfRange,
        QStringLiteral("Index %1 is outside 0..%2").arg(index).arg(childCount));
  }

  if (parentTag == childTag || !child->owned || child->parentTag.has_value()) {
    return failWith(MountError::NotAChildOfParent,
        QStringLiteral("Tag %1 must be an unattached child distinct from its parent").arg(childTag));
  }

  QQuickItem* parentItem = child->viewportModal ? &rootItem_ : parent->item;
  if (!child->viewportModal) {
    // A Flickable scrolls its contentItem, not the viewport itself.
    if (auto* content = parentItem->property("contentItem").value<QQuickItem*>()) {
      parentItem = content;
    }
  }

  child->item->setParentItem(parentItem);
  if (child->viewportModal) {
    modalOrder_.append(childTag);
  } else {
    // Skip logical Modal siblings, which are presented at the viewport.
    for (int siblingIndex = index; siblingIndex < childCount; ++siblingIndex) {
      const auto& sibling = items_[parent->children.at(siblingIndex)];
      if (!sibling.viewportModal) {
        child->item->stackBefore(sibling.item);
        break;
      }
    }
  }
  child->parentTag = parentTag;
  parent->children.insert(index, childTag);
  raiseModals();
#if RNQNX_ENABLE_MODAL_FOCUS
  syncModalFocus();
#endif

  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::remove(
    Tag parentTag,
    Tag childTag,
    int index) {
  const auto parent = items_.find(parentTag);
  if (parent == items_.end()) {
    return failWith(
        MountError::UnknownParent,
        QStringLiteral("Parent tag %1 is not mounted").arg(parentTag));
  }

  const auto child = items_.find(childTag);
  if (child == items_.end()) {
    return failWith(
        MountError::UnknownTag,
        QStringLiteral("Child tag %1 is not mounted").arg(childTag));
  }

  if (child->parentTag != parentTag) {
    return failWith(
        MountError::NotAChildOfParent,
        QStringLiteral("Tag %1 is not a child of tag %2")
            .arg(childTag)
            .arg(parentTag));
  }

  // React Native's Remove carries the index the child occupies. Disagreement
  // means our tree has diverged from the shadow tree, which is a defect worth
  // surfacing rather than papering over.
  const int actualIndex = static_cast<int>(parent->children.indexOf(childTag));
  if (actualIndex != index) {
    return failWith(
        MountError::IndexOutOfRange,
        QStringLiteral("Tag %1 is at index %2, not %3")
            .arg(childTag)
            .arg(actualIndex)
            .arg(index));
  }

  child->parentTag.reset();
  parent->children.removeAt(index);
  modalOrder_.removeOne(childTag);
#if RNQNX_ENABLE_MODAL_FOCUS
  syncModalFocus();
#endif
  child->item->setParentItem(nullptr);
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::destroy(Tag tag) {
  const auto mounted = items_.find(tag);
  if (mounted == items_.end()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  if (!mounted->owned) {
    return failWith(
        MountError::UnknownTag,
        QStringLiteral("Tag %1 is the borrowed root and cannot be deleted")
            .arg(tag));
  }

  QQuickItem* item = mounted->item;
  if (mounted->parentTag) {
    items_[*mounted->parentTag].children.removeOne(tag);
  }
  for (const auto childTag : mounted->children) {
    auto& child = items_[childTag];
    child.parentTag.reset();
    child.item->setParentItem(nullptr);
    modalOrder_.removeOne(childTag);
  }
  modalOrder_.removeOne(tag);
#if RNQNX_ENABLE_MODAL_FOCUS
  mounted->parentTag.reset();
  syncModalFocus();
  modalCloseRequests_.remove(tag);
#endif
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  if (auto* bridge = item->findChild<QtTextInputBridge*>(QString(), Qt::FindDirectChildrenOnly)) bridge->invalidate();
#endif
  items_.erase(mounted);

  // Defensive: a well-formed transaction removes before it deletes.
  if (item->parentItem() != nullptr) {
    item->setParentItem(nullptr);
  }
  delete item;

  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateStyle(Tag tag, const ViewStyle& style) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  QQuickItem* item = mounted->item;
  item->setOpacity(static_cast<qreal>(style.opacity));

  // The QML View is now an Item with either a uniform Rectangle or a per-corner
  // Shape child. Set all properties on the root; the relevant child reads them.
  if (style.hasBackgroundColor) {
    item->setProperty("backgroundColor", QColor::fromRgba(style.backgroundColor));
  }
  item->setProperty("borderWidth", static_cast<qreal>(style.borderWidth));
  item->setProperty("borderColor", QColor::fromRgba(style.borderColor));
  item->setProperty("radius", static_cast<qreal>(style.borderRadius));
  item->setProperty("topLeftRadius", static_cast<qreal>(style.topLeftRadius));
  item->setProperty("topRightRadius", static_cast<qreal>(style.topRightRadius));
  item->setProperty("bottomRightRadius", static_cast<qreal>(style.bottomRightRadius));
  item->setProperty("bottomLeftRadius", static_cast<qreal>(style.bottomLeftRadius));
  item->setProperty("usePerCornerRadius", !style.borderRadiusIsUniform);

  if (style.borderWidth > 0.0F || style.hasBorderColor) {
    // `border` is a grouped property holding a QQuickPen. This is used when the
    // QML View is a plain Rectangle; it is ignored when the per-corner Shape is
    // active.
    if (auto* border = item->property("border").value<QObject*>()) {
      border->setProperty("width", static_cast<qreal>(style.borderWidth));
      if (style.hasBorderColor) {
        border->setProperty("color", QColor::fromRgba(style.borderColor));
      }
    }
  }

  // Apply the simple 2D transform operations. The base position is stored by
  // updateLayout so that translate can be applied on top of the layout frame.
  const auto baseX =
      item->property("layoutX").value<qreal>();
  const auto baseY =
      item->property("layoutY").value<qreal>();
  item->setX(baseX + static_cast<qreal>(style.translateX));
  item->setY(baseY + static_cast<qreal>(style.translateY));
  item->setScale(static_cast<qreal>(style.scale));
  item->setRotation(static_cast<qreal>(style.rotation));

  // Forward accessibility information as dynamic QML properties. These do not
  // enable a screen reader by themselves, but they make the values observable
  // from C++ and QML for future accessibility integration or testing.
  if (style.testId.has_value()) item->setProperty("testID", *style.testId);
  if (style.nativeId.has_value()) item->setProperty("nativeID", *style.nativeId);
  // A retained, inactive React route must block both React pointer hit tests
  // and native Qt controls below it. Keep QML controls' own enabled bindings
  // intact by applying the subtree gate only to the generic View host.
  if (mounted->genericView && style.pointerEventsEnabled.has_value()) {
    item->setEnabled(*style.pointerEventsEnabled);
  }
  item->setProperty("accessible", style.accessible);
  if (style.accessible) {
    item->setProperty(
        "accessibilityLabel",
        style.accessibilityLabel.isEmpty() ? QVariant() : QVariant(style.accessibilityLabel));
    item->setProperty(
        "accessibilityRole",
        style.accessibilityRole.isEmpty() ? QVariant() : QVariant(style.accessibilityRole));
    item->setProperty(
        "accessibilityHint",
        style.accessibilityHint.isEmpty() ? QVariant() : QVariant(style.accessibilityHint));
    item->setProperty(
        "accessibilityValue",
        style.accessibilityValue.isEmpty() ? QVariant() : QVariant(style.accessibilityValue));
    item->setProperty("accessibilityStateDisabled", style.accessibilityStateDisabled);
    item->setProperty("accessibilityStateSelected", style.accessibilityStateSelected);
    item->setProperty("accessibilityStateChecked", style.accessibilityStateChecked);
  }

#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
  if (mounted->genericView) {
    if (!style.accessible) {
      for (const auto* name : {"accessibilityLabel", "accessibilityRole", "accessibilityHint", "accessibilityValue"}) {
        item->setProperty(name, QVariant());
      }
      for (const auto* name : {"accessibilityStateDisabled", "accessibilityStateSelected", "accessibilityStateChecked"}) {
        item->setProperty(name, false);
      }
    }
    QString error;
    if (!applyViewAccessibility(*item, style, error)) {
      return failWith(MountError::UnsupportedAccessibility, error);
    }
  }
#endif
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateTextInput(
    Tag tag,
    const TextInputStyle& style) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  QPointer<QQuickItem> item = mounted->item;
#else
  QQuickItem* item = mounted->item;
#endif

#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  QPointer<QtTextInputBridge> bridge = textInputBridge(tag);
  if (bridge == nullptr) return failWith(MountError::UnsupportedTextInput, QStringLiteral("TextInput editor unavailable"));
  bridge->beginJSUpdate();
  struct EndUpdate {
    QPointer<QtTextInputBridge> bridge;
    bool active{true};
    ~EndUpdate() { if (active && bridge) bridge->cancelJSUpdate(); }
    void finish() {
      active = false; // Observers may throw. Cleanup must not finish twice.
      if (bridge) bridge->endJSUpdate();
    }
  } endUpdate{bridge};
  const auto applyProperties = [&]() -> MountError {
  if (!bridge->configure(style.secure, style.multiline)) {
    return failWith(MountError::UnsupportedTextInput, QStringLiteral("Secure multiline TextInput is unsupported"));
  }
  if (!item || !bridge) return failWith(MountError::UnknownTag, QStringLiteral("TextInput target removed during update"));
  try {
    bridge->applyJS(style.mostRecentEventCount, style.text, style.selectionStart, style.selectionEnd);
  } catch (const std::exception&) {
    return failWith(MountError::IndexOutOfRange, QStringLiteral("Invalid TextInput update"));
  }
  if (!item || !bridge) return failWith(MountError::UnknownTag, QStringLiteral("TextInput target removed during update"));
#else
  // The editable control is inside a wrapper; set text on the actual
  // TextInput/TextEdit so the cursor and user input are not reset.
  QQuickItem* textField = item->findChild<QQuickItem*>(
      style.multiline ? "qnx-text-edit" : "qnx-text-input");
  if (textField && textField->property("text").toString() != style.text) {
    textField->setProperty("text", style.text);
  }
#endif

  item->setProperty("font", style.font);
  item->setProperty("contentInsetLeft", style.contentInsets.left());
  item->setProperty("contentInsetTop", style.contentInsets.top());
  item->setProperty("contentInsetRight", style.contentInsets.right());
  item->setProperty("contentInsetBottom", style.contentInsets.bottom());
  if (style.hasColor) {
    item->setProperty("color", QColor::fromRgba(style.color));
  }

  item->setProperty("readOnly", style.readOnly);
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  if (!item || !bridge) return failWith(MountError::UnknownTag, QStringLiteral("TextInput target removed during update"));
#endif
  // TextInput.Normal is 0 and TextInput.Password is 2
  // (lib/QtQuick.framework/Headers/qquicktextinput_p.h, EchoMode).
  item->setProperty("echoMode", style.secure ? 2 : 0);
  if (style.maxLength > 0) {
    item->setProperty("maximumLength", style.maxLength);
  }
  item->setProperty("multiline", style.multiline);
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  if (!item || !bridge) return failWith(MountError::UnknownTag, QStringLiteral("TextInput target removed during update"));
#endif
  if (!style.placeholder.isEmpty()) {
    item->setProperty("placeholderText", style.placeholder);
  }
  if (style.hasPlaceholderTextColor) {
    item->setProperty(
        "placeholderTextColor", QColor::fromRgba(style.placeholderTextColor));
  }

  lastError_.clear();
  return MountError::None;
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  };
  const auto result = applyProperties();
  const auto detail = lastError_;
  endUpdate.finish(); // All Qt property/focus setter frames have returned.
  if (result != MountError::None) {
    lastError_ = detail;
    return result;
  }
  if (!item || !bridge || itemForTag(tag) != item.data()) {
    return failWith(MountError::UnknownTag, QStringLiteral("TextInput target removed during update"));
  }
  lastError_.clear();
  return MountError::None;
#endif
}

#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
QtTextInputBridge* QtQuickMountingLayer::textInputBridge(Tag tag) {
  auto* item = itemForTag(tag);
  if (item == nullptr || !item->property("text").isValid()) return nullptr;
  if (auto* bridge = item->findChild<QtTextInputBridge*>(QString(), Qt::FindDirectChildrenOnly)) return bridge;
  return new QtTextInputBridge(*item);
}
MountError QtQuickMountingLayer::setTextAndSelection(Tag tag, int count,
    const std::optional<QString>& text, int start, int end) {
  auto* bridge = textInputBridge(tag);
  if (bridge == nullptr) return failWith(MountError::UnknownTag, QStringLiteral("TextInput target unavailable"));
  try { bridge->applyJS(count, text, start, end); }
  catch (const std::exception&) {
    return failWith(MountError::IndexOutOfRange, QStringLiteral("Invalid TextInput update"));
  }
  lastError_.clear();
  return MountError::None;
}
#endif

MountError QtQuickMountingLayer::focusTextInput(Tag tag) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }
  mounted->item->forceActiveFocus();
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::blurTextInput(Tag tag) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }
  mounted->item->setFocus(false);
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateActivityIndicator(
    Tag tag,
    const ActivityIndicatorStyle& style) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  QQuickItem* item = mounted->item;
  item->setProperty("animating", style.animating);
  item->setProperty("hidesWhenStopped", style.hidesWhenStopped);
  item->setProperty("size", style.size);
  if (style.hasColor) {
    item->setProperty("color", QColor::fromRgba(style.color));
  }

  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateSwitch(
    Tag tag,
    const SwitchStyle& style) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  QQuickItem* item = mounted->item;
  item->setProperty("value", style.value);
  item->setProperty("disabled", style.disabled);
  if (style.hasTrackOnColor) {
    item->setProperty("trackOnColor", QColor::fromRgba(style.trackOnColor));
  }
  if (style.hasTrackOffColor) {
    item->setProperty("trackOffColor", QColor::fromRgba(style.trackOffColor));
  }
  if (style.hasThumbColor) {
    item->setProperty("thumbColor", QColor::fromRgba(style.thumbColor));
  }

  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateText(
    Tag tag,
    const TextStyle& style) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  QQuickItem* item = mounted->item;

  // Always reset limits when props are removed; otherwise a reused paragraph
  // can remain elided after numberOfLines returns to its default.
  item->setProperty("maximumLineCount", style.maximumNumberOfLines > 0
      ? style.maximumNumberOfLines : (std::numeric_limits<int>::max)());
  item->setProperty("elide", style.maximumNumberOfLines > 0 ? style.elideMode : -1);
  // The text layout manager measures with wrapping, so the item has to wrap
  // too. Without this the measured height says two lines while the paint says
  // one, and the text runs outside its own frame.
  if (style.maximumNumberOfLines == 1) {
    item->setProperty("wrapMode", 0);  // Text.NoWrap
    item->setProperty("elide", style.elideMode);
  } else if (style.maximumNumberOfLines > 1) {
    item->setProperty("wrapMode", 4);  // Text.Wrap
    item->setProperty("maximumLineCount", style.maximumNumberOfLines);
    item->setProperty("elide", style.elideMode);
  } else {
    item->setProperty("wrapMode", 4);  // Text.Wrap
  }

  // Rich text preserves per-fragment styling; plain text is used otherwise so
  // that markup characters in ordinary strings are never interpreted.
  if (style.mixedStyles) {
    item->setProperty("textFormat", 1);  // Text.RichText
    item->setProperty("text", style.richText);
  } else {
    item->setProperty("textFormat", 0);  // Text.PlainText
    item->setProperty("text", style.text);
  }

  // Apply the resolved QFont used by measurement, including weight, italic,
  // scaling and the actual loaded family. Do not merge with stale item props.
  item->setProperty("font", style.font);
  item->setProperty("lineHeightMode", style.lineHeight > 0 ? 1 : 0);
  item->setProperty("lineHeight", style.lineHeight > 0 ? style.lineHeight : 1.0);
  if (style.hasColor) {
    item->setProperty("color", QColor::fromRgba(style.color));
  }
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateImageSource(
    Tag tag,
    const QString& uri) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  mounted->item->setProperty(
      "source", QUrl(factory_.resolveImageSource(uri)));
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateImageFillMode(Tag tag, int fillMode) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  mounted->item->setProperty("fillMode", fillMode);
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateImageLoadingSource(
    Tag tag,
    const QString& uri) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  mounted->item->setProperty(
      "loadingSource", QUrl(factory_.resolveImageSource(uri)));
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateScrollContentSize(Tag tag) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  auto* content = mounted->item->property("contentItem").value<QQuickItem*>();
  if (content == nullptr) {
    // Not a scrollable item; nothing to compute.
    return MountError::None;
  }

  qreal width = 0;
  qreal height = 0;
  for (const QQuickItem* child : content->childItems()) {
    width = std::max(width, child->x() + child->width());
    height = std::max(height, child->y() + child->height());
  }

  mounted->item->setProperty("contentWidth", width);
  mounted->item->setProperty("contentHeight", height);
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::scrollTo(Tag tag, double x, double y, bool) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  mounted->item->setProperty("contentX", static_cast<qreal>(x));
  mounted->item->setProperty("contentY", static_cast<qreal>(y));
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::scrollToEnd(
    Tag tag,
    bool /*animated*/,
    bool horizontal) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  const auto contentWidth =
      mounted->item->property("contentWidth").toDouble();
  const auto contentHeight =
      mounted->item->property("contentHeight").toDouble();
  const auto itemWidth = mounted->item->width();
  const auto itemHeight = mounted->item->height();

  if (horizontal) {
    mounted->item->setProperty(
        "contentX", std::max(0.0, contentWidth - itemWidth));
  } else {
    mounted->item->setProperty(
        "contentY", std::max(0.0, contentHeight - itemHeight));
  }
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::updateLayout(
    Tag tag,
    const LayoutFrame& frame) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag, QStringLiteral("Tag %1 is not mounted").arg(tag));
  }

  QQuickItem* item = mounted->item;
  // Modal size still comes from Yoga after the native viewport state update.
  // Its logical ancestor's offset and scroll position do not move the portal.
  const qreal x = mounted->viewportModal ? 0 : static_cast<qreal>(frame.x);
  const qreal y = mounted->viewportModal ? 0 : static_cast<qreal>(frame.y);
  item->setProperty("layoutX", x);
  item->setProperty("layoutY", y);
  item->setX(x);
  item->setY(y);
  item->setWidth(static_cast<qreal>(frame.width));
  item->setHeight(static_cast<qreal>(frame.height));

  lastError_.clear();
  return MountError::None;
}

}  // namespace rnqnx::fabric
