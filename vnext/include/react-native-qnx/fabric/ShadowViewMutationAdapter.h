#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"

#include <react/renderer/mounting/ShadowViewMutation.h>

#include <QHash>
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

  /// Maps a React Native component name to a QML type registered on the
  /// factory. Unmapped names fail rather than falling back to a generic view.
  void mapComponent(const QString& componentName, const QString& qmlTypeName);

  /// Applies a transaction in order. The returned vector is empty on success.
  std::vector<MutationFailure> apply(
      const facebook::react::ShadowViewMutation::List& mutations);

 private:
  MountError applyOne(
      const facebook::react::ShadowViewMutation& mutation,
      QString& detail);

  QtQuickMountingLayer& layer_;
  QHash<QString, QString> componentTypes_;
};

/// Converts React Native's view props to the visual style the mounting layer
/// applies. Props that are not ViewProps yield defaults rather than guesses.
ViewStyle toViewStyle(const facebook::react::ShadowView& view);

/// Converts LayoutMetrics::frame (LayoutMetrics.h:26) to the mounting layer's
/// frame type. Both use the platform Float, so no rounding happens here.
LayoutFrame toLayoutFrame(const facebook::react::LayoutMetrics& metrics);

}  // namespace rnqnx::fabric
