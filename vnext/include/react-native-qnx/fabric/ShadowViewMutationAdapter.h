#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"

#include <react/renderer/mounting/ShadowViewMutation.h>

#include <QHash>
#include <QSet>
#include <QString>

#include <vector>

namespace rnqnx::fabric {

/// One rejected mutation, kept with enough context to identify it in a log.
struct MutationFailure {
  facebook::react::ShadowViewMutation::Type type{};
  Tag tag{-1};
  MountError error{MountError::None};
  QString detail;
};

/// Translates a React Native mount transaction into QtQuickMountingLayer calls.
///
/// The switch is over facebook::react::ShadowViewMutation::Type
/// (ReactCommon/react/renderer/mounting/ShadowViewMutation.h:84) and reads only
/// `type`, `parentTag`, `oldChildShadowView`, `newChildShadowView`, and `index`
/// (:94-98). Nothing is inferred: a component name with no registered QML type
/// is a reported failure, never a silently skipped view.
///
/// Mutation order is React Native's, and the adapter preserves it. Failures do
/// not abort the transaction, because stopping halfway would leave the item tree
/// less consistent with the shadow tree than finishing does; every failure is
/// returned instead.
class ShadowViewMutationAdapter {
 public:
  explicit ShadowViewMutationAdapter(QtQuickMountingLayer& layer);

  /// Routes shadow-view event emitters to a touch dispatcher as mutations
  /// arrive. Optional: without one, mounted views simply receive no input.
  ///
  /// The dispatcher is borrowed and must outlive every mutation this adapter
  /// applies — including the empty-tree commit that stopping a surface
  /// produces, which arrives while the host is being torn down. Pass nullptr
  /// before destroying it if that ordering cannot be guaranteed.
  void setTouchDispatcher(QtTouchDispatcher* dispatcher);

  /// Maps a React Native component name to a QML type registered on the
  /// factory. Unmapped names fail rather than falling back to a generic view.
  void mapComponent(const QString& componentName, const QString& qmlTypeName);

  /// Applies a transaction in order. The returned vector is empty on success.
  std::vector<MutationFailure> apply(
      const facebook::react::ShadowViewMutation::List& mutations);

 private:
  void applyText(const facebook::react::ShadowView& view);
  void applyImage(const facebook::react::ShadowView& view);
  void registerEventEmitter(const facebook::react::ShadowView& view);

  /// Connects a scrollable item's position changes to its shadow view's
  /// ScrollViewEventEmitter, so `onScroll` reaches JavaScript.
  void connectScrollEvents(const facebook::react::ShadowView& view);

  MountError applyOne(
      const facebook::react::ShadowViewMutation& mutation,
      QString& detail);

  QtQuickMountingLayer& layer_;
  QtTouchDispatcher* touchDispatcher_{nullptr};
  QHash<QString, QString> componentTypes_;
  /// Tags whose scroll signals are already connected, so a second Update does
  /// not attach a duplicate handler.
  QSet<Tag> scrollConnected_;
};

/// Converts React Native's view props to the visual style the mounting layer
/// applies. Props that are not ViewProps yield defaults rather than guesses.
ViewStyle toViewStyle(const facebook::react::ShadowView& view);

/// Converts LayoutMetrics::frame (LayoutMetrics.h:26) to the mounting layer's
/// frame type. Both use the platform Float, so no rounding happens here.
LayoutFrame toLayoutFrame(const facebook::react::LayoutMetrics& metrics);

}  // namespace rnqnx::fabric
