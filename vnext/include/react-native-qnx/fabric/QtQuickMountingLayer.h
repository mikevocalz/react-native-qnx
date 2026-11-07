#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <cstdint>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

namespace rnqnx::views {
class QtQuickItemFactory;
}

namespace rnqnx::fabric {

/// Mirrors facebook::react::Tag.
/// Installed source: ReactCommon/react/renderer/core/ReactPrimitives.h:17
/// `using Tag = int32_t`.
using Tag = std::int32_t;

/// Mirrors the frame of facebook::react::LayoutMetrics.
/// Installed source: ReactCommon/react/renderer/core/LayoutMetrics.h:26
/// `Rect frame`, whose Rect is graphics/Rect.h:23-25 (`Point origin`,
/// `Size size`), with Point/Size fields typed as
/// graphics/platform/cxx/react/renderer/graphics/Float.h:18 `using Float =
/// float`. QNX uses the generic cxx graphics platform, so these are float and
/// widening to Qt's qreal is explicit at the call site.
struct LayoutFrame {
  float x{0.0F};
  float y{0.0F};
  float width{0.0F};
  float height{0.0F};
};

/// The visual props this platform applies, mirrored from
/// facebook::react::BaseViewProps so the mounting layer stays free of React
/// Native headers.
///
/// Installed sources: `components/view/BaseViewProps.h:46` (opacity), `:47`
/// (backgroundColor), `:50` (borderRadii), `:51` (borderColors), and
/// `components/view/primitives.h:233` (BorderRadii). Colours are 0xAARRGGBB,
/// which is what QColor::fromRgba takes.
struct ViewStyle {
  bool hasBackgroundColor{false};
  std::uint32_t backgroundColor{0};
  float opacity{1.0F};
  float borderRadius{0.0F};
  bool borderRadiusIsUniform{true};
  float topLeftRadius{0.0F};
  float topRightRadius{0.0F};
  float bottomRightRadius{0.0F};
  float bottomLeftRadius{0.0F};
  float borderWidth{0.0F};
  bool borderWidthIsUniform{true};
  bool hasBorderColor{false};
  std::uint32_t borderColor{0};
  bool borderColorIsUniform{true};
  float translateX{0.0F};
  float translateY{0.0F};
  float scale{1.0F};
  float rotation{0.0F};
  bool accessible{false};
  QString accessibilityLabel;
  QString accessibilityRole;
  QString accessibilityHint;
  QString accessibilityValue;
  bool accessibilityStateDisabled{false};
  bool accessibilityStateSelected{false};
  bool accessibilityStateChecked{false};
};

/// The text attributes this platform carries onto a text item. Mirrored from
/// facebook::react::TextAttributes; only the first fragment's values are used,
/// because Qt Quick's `Text` styles the whole item.
struct TextStyle {
  QString text;
  /// The same content with per-fragment styling as Qt rich text. Used when a
  /// paragraph mixes styles, which Qt Quick's `Text` cannot express through its
  /// own font and colour properties.
  QString richText;
  float fontSize{0.0F};
  bool hasColor{false};
  std::uint32_t color{0};
  /// True when the paragraph has more than one styled fragment.
  bool mixedStyles{false};
  /// React Native's `numberOfLines` (zero means no limit).
  int maximumNumberOfLines{0};
  /// Qt Quick `elide` value to use when `maximumNumberOfLines > 0`.
  int elideMode{1};
  QString fontFamily;
};

/// The properties of an editable text field this platform can apply.
///
/// Qt Quick's `TextInput` has no placeholder of its own — that belongs to
/// QtQuick.Controls' `TextField` — so `placeholder` is deliberately absent
/// rather than faked. See `docs/component-support.md`.
struct TextInputStyle {
  QString text;
  float fontSize{0.0F};
  bool hasColor{false};
  std::uint32_t color{0};
  bool hasPlaceholderTextColor{false};
  std::uint32_t placeholderTextColor{0};
  bool readOnly{false};
  bool secure{false};
  bool multiline{false};
  int maxLength{0};
  QString placeholder;
  QString fontFamily;
};

/// The properties of an activity indicator this platform can apply.
struct ActivityIndicatorStyle {
  bool animating{true};
  bool hidesWhenStopped{true};
  bool hasColor{false};
  std::uint32_t color{0};
  QString size;
};

/// The properties of a switch this platform can apply.
struct SwitchStyle {
  bool value{false};
  bool disabled{false};
  bool hasTrackOnColor{false};
  bool hasTrackOffColor{false};
  bool hasThumbColor{false};
  std::uint32_t trackOnColor{0};
  std::uint32_t trackOffColor{0};
  std::uint32_t thumbColor{0};
};

enum class MountError : std::uint8_t {
  None,
  UnknownComponentType,
  DuplicateTag,
  UnknownTag,
  UnknownParent,
  NotAChildOfParent,
  IndexOutOfRange,
  ItemCreationFailed,
};

const char* describe(MountError error);

/// Applies Fabric mount operations to a Qt Quick item tree.
///
/// The operation set is the platform half of the mounting layer. It matches
/// facebook::react::ShadowViewMutation::Type
/// (ReactCommon/react/renderer/mounting/ShadowViewMutation.h:84) one-to-one:
/// Create, Delete, Insert, Remove, Update. The adapter that translates a
/// ShadowViewMutation::List into these calls is a separate translation unit and
/// is not written yet, because the installed React Native headers require folly
/// and glog, which this repository does not vendor. See
/// docs/handoff/NEXT_ACTIONS.md.
///
/// Layout is applied here and only here: Yoga is the layout authority and items
/// are absolutely positioned leaves (ADR-0001). Nothing in this class consults a
/// Qt layout or anchor.
///
/// Every item created through this class is owned by it. Items are constructed
/// unparented — React Native always emits Create before Insert — and a Delete
/// that arrives while the item is still mounted detaches it first rather than
/// leaving a dangling visual child.
class QtQuickMountingLayer {
 public:
  /// \a rootItem is borrowed, never owned, and must outlive this object.
  QtQuickMountingLayer(
      views::QtQuickItemFactory& factory,
      QQuickItem& rootItem,
      Tag rootTag);
  ~QtQuickMountingLayer();

  QtQuickMountingLayer(const QtQuickMountingLayer&) = delete;
  QtQuickMountingLayer& operator=(const QtQuickMountingLayer&) = delete;
  QtQuickMountingLayer(QtQuickMountingLayer&&) = delete;
  QtQuickMountingLayer& operator=(QtQuickMountingLayer&&) = delete;

  MountError create(Tag tag, const QString& componentName);
  MountError insert(Tag parentTag, Tag childTag, int index);
  MountError remove(Tag parentTag, Tag childTag, int index);
  MountError destroy(Tag tag);
  MountError updateLayout(Tag tag, const LayoutFrame& frame);

  /// Applies visual props. Qt Quick's Rectangle carries a single `radius`, so a
  /// per-corner radius collapses to the uniform value; anything React Native
  /// expresses that Rectangle cannot is an honest gap in
  /// docs/component-support.md rather than a silent approximation.
  MountError updateStyle(Tag tag, const ViewStyle& style);

  /// Sets the string a text item displays. Qt Quick's `Text` exposes `text` as
  /// a QML property, so this goes through the metaobject like the other visual
  /// props; a component whose QML source has no `text` property is unaffected.
  MountError updateText(Tag tag, const TextStyle& style);

  /// Applies editable-field properties. The item must expose Qt Quick's
  /// `TextInput` surface; a plain Item silently ignores the unknown properties,
  /// so the mapping is what keeps this honest rather than a type check.
  MountError updateTextInput(Tag tag, const TextInputStyle& style);

  /// Gives a mounted text field active focus.
  MountError focusTextInput(Tag tag);

  /// Removes active focus from a mounted text field.
  MountError blurTextInput(Tag tag);

  /// Applies activity-indicator properties. The QML item exposes `color`,
  /// `animating`, `hidesWhenStopped`, and `size`.
  MountError updateActivityIndicator(Tag tag, const ActivityIndicatorStyle& style);

  /// Applies switch properties. The QML item exposes `value`, `disabled`,
  /// `onTintColor`, `thumbTintColor`, and `tintColor`.
  MountError updateSwitch(Tag tag, const SwitchStyle& style);

  /// Points an image item at a source URI. `file:` and `http(s):` are fetched by
  /// Qt; `data:` URIs are decoded by this platform and served back through its
  /// image provider, because Qt Quick's `Image` does not read them.
  MountError updateImageSource(Tag tag, const QString& uri);

  /// Sets Qt Quick's `Image.fillMode` (Stretch=0, PreserveAspectFit=1,
  /// PreserveAspectCrop=2) from React Native's `resizeMode`.
  MountError updateImageFillMode(Tag tag, int fillMode);

  /// Points the loading-indicator image at a source URI. This is shown while
  /// the primary source is still loading.
  MountError updateImageLoadingSource(Tag tag, const QString& uri);

  /// Scrolls a mounted `Flickable` to an absolute `(x, y)` offset. `animated`
  /// is accepted but currently ignored — QML content position is set directly.
  MountError scrollTo(Tag tag, double x, double y, bool /*animated*/);

  /// Scrolls a mounted `Flickable` to the bottom (or right, if `horizontal`) of
  /// its content. `animated` is accepted but ignored.
  MountError scrollToEnd(Tag tag, bool /*animated*/, bool horizontal);

  /// Recomputes a scrollable item's content size from its mounted children.
  /// Qt's `Flickable` will not scroll past `contentWidth`/`contentHeight`, and
  /// unlike React Native it does not infer them from its children.
  MountError updateScrollContentSize(Tag tag);

  /// Borrowed; nullptr when the tag is not mounted.
  QQuickItem* itemForTag(Tag tag) const;

  /// Number of tags currently known, including the root.
  int mountedCount() const;

  /// Detail for the most recent failure, empty after a success.
  QString lastError() const;

 private:
  struct MountedItem {
    QQuickItem* item{nullptr};
    bool owned{false};
  };

  QList<QQuickItem*> mountedChildren(QQuickItem* parent) const;
  MountError failWith(MountError error, const QString& detail);

  views::QtQuickItemFactory& factory_;
  QQuickItem& rootItem_;
  Tag rootTag_;
  QHash<Tag, MountedItem> items_;
  // Non-owning membership index, kept in step with the tag registry.
  QSet<QQuickItem*> mountedPointers_;
  QString lastError_;
};

}  // namespace rnqnx::fabric
