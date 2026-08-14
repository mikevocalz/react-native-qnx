#pragma once

#include <QHash>
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
  float borderWidth{0.0F};
  bool hasBorderColor{false};
  std::uint32_t borderColor{0};
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

  MountError failWith(MountError error, const QString& detail);

  views::QtQuickItemFactory& factory_;
  QQuickItem& rootItem_;
  Tag rootTag_;
  QHash<Tag, MountedItem> items_;
  QString lastError_;
};

}  // namespace rnqnx::fabric
