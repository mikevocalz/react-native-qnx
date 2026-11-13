#pragma once

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"

#include <react/renderer/mounting/ShadowViewMutation.h>

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>

#include "react-native-qnx/fabric/QtModalBridge.h"

#include <vector>

namespace rnqnx::fabric {

/// One rejected mutation, kept with enough context to identify it in a log.
struct MutationFailure {
  facebook::react::ShadowViewMutation::Type type{};
  Tag tag{-1};
  MountError error{MountError::None};
  QString detail;
};

class QtAccessibilityActivation;
class QtImageBridge;
class QtSwitchBridge;
class QtSvgBridge;

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
  /// Call after the surface root changes size (window resize, maximize):
  /// open modals are resized to cover the new surface.
  void surfaceResized();

  explicit ShadowViewMutationAdapter(QtQuickMountingLayer& layer);
  ~ShadowViewMutationAdapter();
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
  ShadowViewMutationAdapter(const ShadowViewMutationAdapter&) = delete;
  ShadowViewMutationAdapter& operator=(const ShadowViewMutationAdapter&) = delete;
#endif

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

  /// Dispatches an imperative command to a mounted item, such as `ScrollView`'s
  /// `scrollTo` or `scrollToEnd`.
  MutationFailure dispatchCommand(
      facebook::react::Tag tag,
      const std::string& commandName,
      const folly::dynamic& args,
      const facebook::react::ShadowView* shadowView);

  /// Applies a props delta to an already-mounted view. This is the synchronous
  /// path the `NativeAnimated` backend uses for direct property updates such
  /// as `opacity` and `transform`.
  void synchronouslyUpdateViewOnUIThread(
      facebook::react::Tag tag, const folly::dynamic& props);

 private:
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
  MountError applyAccessibilityActions(const facebook::react::ShadowView& view, QString& detail);
  QHash<Tag, QPointer<QtAccessibilityActivation>> accessibilityActivations_;
#endif
  void applyText(const facebook::react::ShadowView& view);
  void applyImage(const facebook::react::ShadowView& view);
  MountError applyTextInput(const facebook::react::ShadowView& view);
  void applyActivityIndicator(const facebook::react::ShadowView& view);
  void applySwitch(const facebook::react::ShadowView& view);
  void connectModalEvents(const facebook::react::ShadowView& view);
  MountError applyModal(const facebook::react::ShadowView& view);
#if RNQNX_ENABLE_QT_SVG
  void applySvgDocument(const facebook::react::ShadowView& view);
  QHash<Tag, QPointer<QtSvgBridge>> svgBridges_;
#endif
  void registerEventEmitter(const facebook::react::ShadowView& view);

  /// Connects an editable item's text changes to its shadow view's
  /// TextInputEventEmitter, so `onChange` reaches JavaScript.
  void connectTextInputEvents(const facebook::react::ShadowView& view);

  /// Connects a scrollable item's position changes to its shadow view's
  /// ScrollViewEventEmitter, so `onScroll` reaches JavaScript.
  void connectScrollEvents(const facebook::react::ShadowView& view);

  /// Connects a switch item's value changes to its shadow view's
  /// SwitchEventEmitter, so `onValueChange` reaches JavaScript.
  void connectSwitchEvents(const facebook::react::ShadowView& view);

  /// Connects an image item's status changes to its shadow view's
  /// ImageEventEmitter, so `onLoad`, `onError`, etc. reach JavaScript.
  void connectImageEvents(const facebook::react::ShadowView& view);

  MountError applyOne(
      const facebook::react::ShadowViewMutation& mutation,
      QString& detail);

  QtQuickMountingLayer& layer_;
  QtTouchDispatcher* touchDispatcher_{nullptr};
  QHash<QString, QString> componentTypes_;
  /// Tags whose scroll signals are already connected, so a second Update does
  /// not attach a duplicate handler.
  QSet<Tag> scrollConnected_;
  /// Tags whose text-change signal is already connected, for the same reason.
  QSet<Tag> textInputConnected_;
  /// Switch bridges, indexed by tag, so `applySwitch` can pre-arm the guard.
  QHash<Tag, QtSwitchBridge*> switchBridges_;
  /// Image bridges, indexed by tag, so load/error events reach JavaScript.
  QHash<Tag, QtImageBridge*> imageBridges_;
  // ModalHostView: React Native's Modal, drawn as an overlay over the whole
  // surface and sized through its state the way iOS and Android size it.
  QHash<Tag, facebook::react::ShadowView> modalViews_;
  /// Escape and focus bridges, owned by their modal items.
  QHash<Tag, QPointer<QtModalBridge>> modalBridges_;
};

/// Converts React Native's view props to the visual style the mounting layer
/// applies. Props that are not ViewProps yield defaults rather than guesses.
ViewStyle toViewStyle(const facebook::react::ShadowView& view);

/// Converts LayoutMetrics::frame (LayoutMetrics.h:26) to the mounting layer's
/// frame type. Both use the platform Float, so no rounding happens here.
LayoutFrame toLayoutFrame(const facebook::react::LayoutMetrics& metrics);

}  // namespace rnqnx::fabric
