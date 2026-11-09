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
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QHash>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QTouchEvent>
#include <QWheelEvent>

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

// Directories searched for bundled/native font files, in addition to system
// font locations. Qt QNX deployments usually ship assets under the executable.
QStringList defaultFontSearchPaths() {
  QStringList paths;
  paths << QCoreApplication::applicationDirPath() + QStringLiteral("/assets/fonts");
  paths << QCoreApplication::applicationDirPath() + QStringLiteral("/fonts");
  for (const auto& path : QStandardPaths::standardLocations(
           QStandardPaths::FontsLocation)) {
    paths << path;
  }
  return paths;
}

// Try to load a font file whose name matches the requested family. Returns the
// actual font-family name reported by Qt, or the requested family if no file
// could be loaded. Loading is attempted once per family per process and the
// result is cached.
QString resolveFontFamily(const QString& requested) {
  if (requested.isEmpty()) {
    return requested;
  }

  static QHash<QString, QString> resolved;
  if (resolved.contains(requested)) {
    return resolved.value(requested);
  }

  const QStringList exts = {
      QStringLiteral(".ttf"),
      QStringLiteral(".otf"),
      QStringLiteral(".ttc"),
  };

  for (const auto& dir : defaultFontSearchPaths()) {
    if (!QDir(dir).exists()) {
      continue;
    }
    for (const auto& ext : exts) {
      const QString fileName = requested + ext;
      const QString lowerFileName = requested.toLower() + ext;
      for (const auto& name : {fileName, lowerFileName}) {
        const QString path = dir + QLatin1Char('/') + name;
        if (QFile::exists(path)) {
          const int id = QFontDatabase::addApplicationFont(path);
          if (id >= 0) {
            const QStringList families =
                QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) {
              resolved.insert(requested, families.first());
              return families.first();
            }
          }
        }
      }
    }
  }

  resolved.insert(requested, requested);
  return requested;
}

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
  }
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
  mounted.genericView = componentName == QStringLiteral("View");
#endif
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

  child->item->setParentItem(nullptr);
  child->parentTag.reset();
  parent->children.removeAt(index);
  modalOrder_.removeOne(childTag);
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

  if (style.fontSize > 0.0F || !style.fontFamily.isEmpty()) {
    QFont font = item->property("font").value<QFont>();
    if (style.fontSize > 0.0F) {
      font.setPixelSize(static_cast<int>(style.fontSize));
    }
    if (!style.fontFamily.isEmpty()) {
      font.setFamily(resolveFontFamily(style.fontFamily));
    }
    item->setProperty("font", font);
  }
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

  if (style.fontSize > 0.0F || !style.fontFamily.isEmpty()) {
    // `font` is a grouped property holding a QFont value, so it is read,
    // modified, and written back rather than set through a dotted name.
    QFont font = item->property("font").value<QFont>();
    if (style.fontSize > 0.0F) {
      font.setPixelSize(static_cast<int>(style.fontSize));
    }
    if (!style.fontFamily.isEmpty()) {
      font.setFamily(resolveFontFamily(style.fontFamily));
    }
    item->setProperty("font", font);
  }
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
