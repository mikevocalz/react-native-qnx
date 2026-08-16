#include "react-native-qnx/fabric/QtQuickMountingLayer.h"

#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QColor>
#include <QFont>
#include <QUrl>

#include <algorithm>
#include <QList>
#include <QQuickItem>

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

MountError QtQuickMountingLayer::failWith(
    MountError error,
    const QString& detail) {
  lastError_ = detail;
  return error;
}

MountError QtQuickMountingLayer::create(
    Tag tag,
    const QString& componentName) {
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

  items_.insert(tag, MountedItem{item, true});
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::insert(
    Tag parentTag,
    Tag childTag,
    int index) {
  const auto parent = items_.constFind(parentTag);
  if (parent == items_.constEnd()) {
    return failWith(
        MountError::UnknownParent,
        QStringLiteral("Parent tag %1 is not mounted").arg(parentTag));
  }

  const auto child = items_.constFind(childTag);
  if (child == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag,
        QStringLiteral("Child tag %1 is not mounted").arg(childTag));
  }

  // A Flickable scrolls its `contentItem`, not itself, so children mounted
  // directly on it would never move. Any item exposing a `contentItem` gets its
  // children there; a plain Item has no such property and is unaffected.
  QQuickItem* parentItem = parent->item;
  if (auto* content = parentItem->property("contentItem").value<QQuickItem*>()) {
    parentItem = content;
  }

  const int childCount = static_cast<int>(parentItem->childItems().size());
  if (index < 0 || index > childCount) {
    return failWith(
        MountError::IndexOutOfRange,
        QStringLiteral("Index %1 is outside 0..%2").arg(index).arg(childCount));
  }

  child->item->setParentItem(parentItem);

  // setParentItem appends, so the item only needs restacking when it is not
  // already last. childItems() order is the stacking order Qt paints in.
  const QList<QQuickItem*> children = parentItem->childItems();
  if (index < children.size() - 1) {
    child->item->stackBefore(children.at(index));
  }

  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::remove(
    Tag parentTag,
    Tag childTag,
    int index) {
  const auto parent = items_.constFind(parentTag);
  if (parent == items_.constEnd()) {
    return failWith(
        MountError::UnknownParent,
        QStringLiteral("Parent tag %1 is not mounted").arg(parentTag));
  }

  const auto child = items_.constFind(childTag);
  if (child == items_.constEnd()) {
    return failWith(
        MountError::UnknownTag,
        QStringLiteral("Child tag %1 is not mounted").arg(childTag));
  }

  QQuickItem* parentItem = parent->item;
  if (auto* content = parentItem->property("contentItem").value<QQuickItem*>()) {
    parentItem = content;
  }

  if (child->item->parentItem() != parentItem) {
    return failWith(
        MountError::NotAChildOfParent,
        QStringLiteral("Tag %1 is not a child of tag %2")
            .arg(childTag)
            .arg(parentTag));
  }

  // React Native's Remove carries the index the child occupies. Disagreement
  // means our tree has diverged from the shadow tree, which is a defect worth
  // surfacing rather than papering over.
  const int actualIndex =
      static_cast<int>(parentItem->childItems().indexOf(child->item));
  if (actualIndex != index) {
    return failWith(
        MountError::IndexOutOfRange,
        QStringLiteral("Tag %1 is at index %2, not %3")
            .arg(childTag)
            .arg(actualIndex)
            .arg(index));
  }

  child->item->setParentItem(nullptr);
  lastError_.clear();
  return MountError::None;
}

MountError QtQuickMountingLayer::destroy(Tag tag) {
  const auto mounted = items_.constFind(tag);
  if (mounted == items_.constEnd()) {
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

  // These are QML properties of Rectangle rather than QQuickItem members, so
  // they go through the metaobject. A component type whose QML source is not a
  // Rectangle simply will not have them, and setProperty is a no-op there.
  if (style.hasBackgroundColor) {
    item->setProperty("color", QColor::fromRgba(style.backgroundColor));
  }
  item->setProperty("radius", static_cast<qreal>(style.borderRadius));

  if (style.borderWidth > 0.0F || style.hasBorderColor) {
    // `border` is a grouped property holding a QQuickPen.
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

  QQuickItem* item = mounted->item;

  // The editable control is inside a wrapper; set text on the actual
  // TextInput/TextEdit so the cursor and user input are not reset.
  QQuickItem* textField = item->findChild<QQuickItem*>(
      style.multiline ? "qnx-text-edit" : "qnx-text-input");
  if (textField && textField->property("text").toString() != style.text) {
    textField->setProperty("text", style.text);
  }

  if (style.fontSize > 0.0F) {
    QFont font = item->property("font").value<QFont>();
    font.setPixelSize(static_cast<int>(style.fontSize));
    item->setProperty("font", font);
  }
  if (style.hasColor) {
    item->setProperty("color", QColor::fromRgba(style.color));
  }

  item->setProperty("readOnly", style.readOnly);
  // TextInput.Normal is 0 and TextInput.Password is 2
  // (lib/QtQuick.framework/Headers/qquicktextinput_p.h, EchoMode).
  item->setProperty("echoMode", style.secure ? 2 : 0);
  if (style.maxLength > 0) {
    item->setProperty("maximumLength", style.maxLength);
  }
  item->setProperty("multiline", style.multiline);
  if (!style.placeholder.isEmpty()) {
    item->setProperty("placeholderText", style.placeholder);
  }
  if (style.hasPlaceholderTextColor) {
    item->setProperty(
        "placeholderTextColor", QColor::fromRgba(style.placeholderTextColor));
  }

  lastError_.clear();
  return MountError::None;
}

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

  if (style.fontSize > 0.0F) {
    // `font` is a grouped property holding a QFont value, so it is read,
    // modified, and written back rather than set through a dotted name.
    QFont font = item->property("font").value<QFont>();
    font.setPixelSize(static_cast<int>(style.fontSize));
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
  item->setProperty("layoutX", static_cast<qreal>(frame.x));
  item->setProperty("layoutY", static_cast<qreal>(frame.y));
  item->setX(static_cast<qreal>(frame.x));
  item->setY(static_cast<qreal>(frame.y));
  item->setWidth(static_cast<qreal>(frame.width));
  item->setHeight(static_cast<qreal>(frame.height));

  lastError_.clear();
  return MountError::None;
}

}  // namespace rnqnx::fabric
