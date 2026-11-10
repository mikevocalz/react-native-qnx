#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
#include "react-native-qnx/fabric/QtAccessibilityActivation.h"
#endif
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
#include "react-native-qnx/fabric/QtTextInputBridge.h"
#endif

#include "react-native-qnx/fabric/QtImageBridge.h"
#include "react-native-qnx/fabric/QtScrollBridge.h"
#include "react-native-qnx/fabric/QtSwitchBridge.h"
#include "react-native-qnx/fabric/QtViewFocusBridge.h"
#include "react-native-qnx/text/QtTextAttributes.h"

#include <react/renderer/components/iostextinput/TextInputProps.h>
#include <react/renderer/components/textinput/TextInputEventEmitter.h>

#include <QColor>
#include <QKeyEvent>
#include <QList>
#include <QMetaMethod>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/components/FBReactNativeSpec/EventEmitters.h>
#include <react/renderer/components/FBReactNativeSpec/Props.h>
#include <react/renderer/components/image/ImageEventEmitter.h>
#include <react/renderer/components/scrollview/BaseScrollViewProps.h>
#include <react/renderer/components/image/ImageProps.h>
#include <react/renderer/components/modal/ModalHostViewState.h>
#include <react/renderer/components/scrollview/ScrollViewEventEmitter.h>
#include <react/renderer/components/view/ViewEventEmitter.h>
#include <react/renderer/components/text/BaseParagraphProps.h>
#include <react/renderer/components/text/ParagraphState.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/ConcreteState.h>
#include <react/renderer/graphics/Color.h>
#include <react/renderer/mounting/ShadowView.h>

namespace rnqnx::fabric {

// Match Fabric's native Modal state seam: viewport changes update state, then
// ModalHostViewComponentDescriptor gives those dimensions to Yoga. The async
// state callback owns only copied data and cannot outlive a Qt pointer.
class QtModalBridge final : public QObject {
 public:
  using State = facebook::react::ConcreteState<facebook::react::ModalHostViewState>;
  enum class FocusRestoration { Restore, Skip };

  QtModalBridge(QQuickItem& item, QQuickItem& viewport)
      : QObject(&item), item_(item), viewport_(viewport) {
    QObject::connect(&viewport_, &QQuickItem::widthChanged, this, [this] { updateViewport(); });
    QObject::connect(&viewport_, &QQuickItem::heightChanged, this, [this] { updateViewport(); });
    QObject::connect(&item_, &QQuickItem::windowChanged, this,
        [this](QQuickWindow* window) { if (mounted_) observeWindow(window); });
    QObject::connect(&viewport_, &QQuickItem::enabledChanged, this,
        [this] { if (viewport_.isEnabled()) queueInitialFocus(); });
  }

  void update(State::Shared state,
      std::shared_ptr<const facebook::react::ModalHostViewEventEmitter> emitter) {
    state_ = std::move(state);
    emitter_ = std::move(emitter);
    updateViewport();
    if (!initialFocusChosen_) queueInitialFocus();
  }

  void setMounted(bool mounted, FocusRestoration restoration = FocusRestoration::Restore) {
#if RNQNX_ENABLE_MODAL_FOCUS
    // The window-wide owner coordinates focus across Fabric surfaces. Keep
    // this bridge responsible only for modal state and sizing in that mode.
    Q_UNUSED(restoration);
    mounted_ = mounted;
    return;
#else
    if (mounted_ == mounted) return;
    const bool wasTopmost = isTopmost();
    const auto previousWindow = window_;
    const auto previous = previousFocus_;
    mounted_ = mounted;
    item_.setProperty(kModalMarker, mounted);
    initialFocusChosen_ = false;
    if (mounted_) {
      observeWindow(item_.window());
      return;
    }
    observeWindow(nullptr);
    previousFocus_.clear();
    if (restoration == FocusRestoration::Skip || !wasTopmost || !previousWindow) return;
    const QPointer<QQuickItem> viewport(&viewport_);
    const QPointer<QQuickItem> closing(&item_);
    // The closing receiver is about to be destroyed. Bind the return to the
    // still-live window and hold only guarded item references across the turn.
    QTimer::singleShot(0, previousWindow, [previousWindow, previous, viewport, closing] {
      if (!previousWindow || !viewport || !viewport->isEnabled() || !viewport->isVisible()) return;
      auto* modal = topmost(*viewport);
      if (modal && modal == closing) return; // A remove/insert move reopened it.
      auto* current = previousWindow->activeFocusItem();
      if (modal) {
        if (contains(*modal, current)) return;
        if (previous && contains(*modal, previous) && previous->isVisible() && previous->isEnabled()) {
          previous->forceActiveFocus(Qt::OtherFocusReason);
        } else {
          focusFirst(*modal);
        }
        return;
      }
      // A React postcommit callback may already have selected the appropriate
      // opener after a breakpoint change. Respect that explicit current focus.
      if (current && current != previousWindow->contentItem() && current != viewport &&
          (!closing || !contains(*closing, current)) && current->isVisible() && current->isEnabled()) return;
      if (previous && previous->window() == previousWindow && previous->isVisible() && previous->isEnabled()) {
        previous->forceActiveFocus(Qt::OtherFocusReason);
      }
    });
#endif
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
#if !RNQNX_ENABLE_MODAL_FOCUS
    if (watched == window_ && isTopmost() &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
      const auto* key = static_cast<QKeyEvent*>(event);
      if (event->type() == QEvent::KeyPress && key->key() == Qt::Key_Escape &&
          key->modifiers() == Qt::NoModifier && emitter_) {
        // Native dismissal is a request; the app's existing callback decides
        // whether to change visible. The latest inserted Modal filters first.
        const auto emitter = emitter_;
        if (!key->isAutoRepeat()) emitter->onRequestClose({});
        event->accept();
        return true;
      }
      if ((key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) &&
          (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier)) {
        if (event->type() == QEvent::KeyPress) {
          focusNext(key->key() == Qt::Key_Backtab || key->modifiers() == Qt::ShiftModifier);
        }
        event->accept();
        return true;
      }
      // Native focus can be changed imperatively by the app. An open modal
      // must still prevent typing or activation reaching a control behind it.
      if (!contains(item_, window_->activeFocusItem())) {
        focusFirst(item_);
        event->accept();
        return true;
      }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif
    return QObject::eventFilter(watched, event);
  }

 private:
  static constexpr const char* kModalMarker = "rnqnxViewportModal";

  static bool contains(const QQuickItem& root, QQuickItem* item) {
    return item && (item == &root || root.isAncestorOf(item));
  }

  static QQuickItem* topmost(QQuickItem& viewport) {
    // The mounting layer portals modals into this viewport and raises them in
    // insertion order. Nested RN modals are siblings in this visual ordering.
    const auto children = viewport.childItems();
    for (auto it = children.crbegin(); it != children.crend(); ++it) {
      auto* item = *it;
      if (item->property(kModalMarker).toBool() && item->isVisible() && item->isEnabled()) return item;
    }
    return nullptr;
  }

  static void collectFocusable(QQuickItem& root, QList<QQuickItem*>& result) {
    for (auto* child : root.childItems()) {
      if (!child->isVisible() || !child->isEnabled()) continue;
      if (child->activeFocusOnTab()) result.append(child);
      collectFocusable(*child, result);
    }
  }

  static void focusFirst(QQuickItem& modal) {
    QList<QQuickItem*> candidates;
    collectFocusable(modal, candidates);
    (candidates.isEmpty() ? &modal : candidates.front())->forceActiveFocus(Qt::OtherFocusReason);
  }

  bool isTopmost() const {
    return mounted_ && window_ && viewport_.isVisible() && viewport_.isEnabled() &&
        item_.isVisible() && item_.isEnabled() && topmost(viewport_) == &item_;
  }

  void focusNext(bool backwards) {
    QList<QQuickItem*> candidates;
    collectFocusable(item_, candidates);
    if (candidates.isEmpty()) {
      item_.forceActiveFocus(Qt::OtherFocusReason);
      return;
    }
    qsizetype currentIndex = -1;
    auto* current = window_->activeFocusItem();
    for (qsizetype index = 0; index < candidates.size(); ++index) {
      if (contains(*candidates[index], current)) currentIndex = index;
    }
    const auto next = currentIndex < 0
        ? (backwards ? candidates.size() - 1 : 0)
        : (currentIndex + (backwards ? -1 : 1) + candidates.size()) % candidates.size();
    candidates[next]->forceActiveFocus(backwards ? Qt::BacktabFocusReason : Qt::TabFocusReason);
  }

  void queueInitialFocus() {
#if !RNQNX_ENABLE_MODAL_FOCUS
    if (!mounted_ || focusPending_) return;
    focusPending_ = true;
    QTimer::singleShot(0, this, [this] {
      focusPending_ = false;
      if (!isTopmost()) return;
      auto* current = window_->activeFocusItem();
      // JS can deliberately select a navigation row before this queued turn.
      if (current != &item_ && contains(item_, current)) {
        initialFocusChosen_ = true;
        return;
      }
      focusFirst(item_);
      initialFocusChosen_ = window_->activeFocusItem() != &item_;
    });
#endif
  }

  void observeWindow(QQuickWindow* window) {
#if !RNQNX_ENABLE_MODAL_FOCUS
    if (window_) window_->removeEventFilter(this);
    window_ = window;
    if (window_) {
      previousFocus_ = window_->activeFocusItem();
      window_->installEventFilter(this);
      queueInitialFocus();
    }
#else
    Q_UNUSED(window);
#endif
  }

  void updateViewport() {
    if (!state_) return;
    const facebook::react::Size size{
        .width = static_cast<facebook::react::Float>(viewport_.width()),
        .height = static_cast<facebook::react::Float>(viewport_.height())};
    if (requestedSize_ == size) return;
    requestedSize_ = size;
    state_->updateState([size](const facebook::react::ModalHostViewState& previous)
        -> State::SharedData {
      if (previous.screenSize == size) return nullptr;
      return std::make_shared<const facebook::react::ModalHostViewState>(size);
    });
  }

  QQuickItem& item_;
  QQuickItem& viewport_;
  QPointer<QQuickWindow> window_;
  QPointer<QQuickItem> previousFocus_;
  bool mounted_ = false;
  bool focusPending_ = false;
  bool initialFocusChosen_ = false;
  State::Shared state_;
  std::shared_ptr<const facebook::react::ModalHostViewEventEmitter> emitter_;
  std::optional<facebook::react::Size> requestedSize_{std::nullopt};
};

namespace {

using facebook::react::ShadowView;
using facebook::react::ShadowViewMutation;

/// ShadowView::componentName is a `const char *` (ShadowView.h:41) that points
/// at a static string owned by the component descriptor, so copying it into a
/// QString is safe and keeps the map key stable.
QString componentNameOf(const ShadowView& view) {
  return view.componentName == nullptr ? QString()
                                       : QString::fromUtf8(view.componentName);
}

/// Packs a React Native colour into 0xAARRGGBB.
/// Seams: graphics/Color.h:71-74 (component accessors), :67 (meaningfulness).
std::uint32_t packColor(const facebook::react::SharedColor& color) {
  return (static_cast<std::uint32_t>(facebook::react::alphaFromColor(color))
          << 24) |
      (static_cast<std::uint32_t>(facebook::react::redFromColor(color)) << 16) |
      (static_cast<std::uint32_t>(facebook::react::greenFromColor(color))
       << 8) |
      static_cast<std::uint32_t>(facebook::react::blueFromColor(color));
}

TextStyle paragraphTextStyle(const ShadowView& view) {
  using ParagraphState = facebook::react::ParagraphState;
  const auto* state =
      dynamic_cast<const facebook::react::ConcreteState<ParagraphState>*>(
          view.state.get());
  TextStyle style;
  if (state == nullptr) {
    return style;
  }
  const auto& attributedString = state->getData().attributedString;
  style.font = rnqnx::text::fontFromTextAttributes(attributedString.getBaseTextAttributes());
  style.lineHeight = rnqnx::text::paragraphLineHeight(attributedString);

  bool first = true;
  int fragmentCount = 0;
  for (const auto& fragment : attributedString.getFragments()) {
    if (fragment.isAttachment()) {
      continue;
    }
    ++fragmentCount;
    style.text += QString::fromStdString(fragment.string);

    // Per-fragment styling is carried as rich text, because Qt Quick's Text
    // applies its own font and colour to the whole item. The plain string is
    // kept alongside it for anything that reads text without markup.
    style.richText += rnqnx::text::fragmentMarkup(fragment);

    if (first) {
      first = false;
      style.font = rnqnx::text::fontFromTextAttributes(fragment.textAttributes);
      if (facebook::react::isColorMeaningful(
              fragment.textAttributes.foregroundColor)) {
        style.hasColor = true;
        style.color = packColor(fragment.textAttributes.foregroundColor);
      }
    }
  }

  // More than one styled fragment means Qt Quick's single font and colour
  // cannot express the paragraph, so the rich-text form is used instead.
  style.mixedStyles = fragmentCount > 1;

  const auto* paragraphProps =
      dynamic_cast<const facebook::react::BaseParagraphProps*>(
          view.props.get());
  if (paragraphProps != nullptr) {
    style.maximumNumberOfLines =
        paragraphProps->paragraphAttributes.maximumNumberOfLines;
    switch (paragraphProps->paragraphAttributes.ellipsizeMode) {
      case facebook::react::EllipsizeMode::Head:
        style.elideMode = 0;  // Text.ElideLeft
        break;
      case facebook::react::EllipsizeMode::Tail:
        style.elideMode = 1;  // Text.ElideRight
        break;
      case facebook::react::EllipsizeMode::Middle:
        style.elideMode = 2;  // Text.ElideMiddle
        break;
      case facebook::react::EllipsizeMode::Clip:
        style.elideMode = -1;  // Text.ElideNone (clip without ellipsis)
        break;
    }
  }

  return style;
}

}  // namespace

ViewStyle toViewStyle(const ShadowView& view) {
  ViewStyle style;

  const auto* viewProps =
      dynamic_cast<const facebook::react::ViewProps*>(view.props.get());
  if (viewProps == nullptr) {
    // A component whose props are not ViewProps has no visual style this
    // platform knows how to apply. Defaults are returned rather than guesses.
    return style;
  }

  style.opacity = static_cast<float>(viewProps->opacity);

  if (facebook::react::isColorMeaningful(viewProps->backgroundColor)) {
    style.hasBackgroundColor = true;
    style.backgroundColor = packColor(viewProps->backgroundColor);
  } else if (dynamic_cast<const facebook::react::TextInputProps*>(view.props.get()) != nullptr) {
    // This is a complete Fabric props snapshot. Removing an input's fill
    // restores its transparent default; sparse animated updates stay separate.
    style.hasBackgroundColor = true;
    style.backgroundColor = 0;
  }

  // React Native resolves the border cascade itself, including percentage radii
  // against the frame (BaseViewProps.h:115). Doing it here by hand would be a
  // second implementation of the same rules.
  const auto border = viewProps->resolveBorderMetrics(view.layoutMetrics);

  // Qt Quick's Rectangle carries a single radius, so a per-corner radius
  // collapses. When all four agree we use that value; otherwise we fall back to
  // the largest radius so rounded corners still appear (the exact per-corner
  // shape would need a custom QML path).
  const float topLeftRadius =
      static_cast<float>(border.borderRadii.topLeft.horizontal);
  const float topRightRadius =
      static_cast<float>(border.borderRadii.topRight.horizontal);
  const float bottomRightRadius =
      static_cast<float>(border.borderRadii.bottomRight.horizontal);
  const float bottomLeftRadius =
      static_cast<float>(border.borderRadii.bottomLeft.horizontal);
  const bool uniformRadius =
      (topLeftRadius == topRightRadius) &&
      (topRightRadius == bottomRightRadius) &&
      (bottomRightRadius == bottomLeftRadius);
  style.borderRadius = uniformRadius
      ? topLeftRadius
      : std::max({topLeftRadius, topRightRadius, bottomRightRadius, bottomLeftRadius});
  style.borderRadiusIsUniform = uniformRadius;
  style.topLeftRadius = topLeftRadius;
  style.topRightRadius = topRightRadius;
  style.bottomRightRadius = bottomRightRadius;
  style.bottomLeftRadius = bottomLeftRadius;

  // Qt Quick's Rectangle has a single border width and colour. If all four
  // edges agree we can represent them faithfully; otherwise we collapse to the
  // top edge and record the lossy case in component-support.md.
  const float topWidth = static_cast<float>(border.borderWidths.top);
  const bool uniformWidth =
      (border.borderWidths.top == border.borderWidths.right) &&
      (border.borderWidths.right == border.borderWidths.bottom) &&
      (border.borderWidths.bottom == border.borderWidths.left);
  style.borderWidth = uniformWidth
      ? topWidth
      : static_cast<float>(border.borderWidths.top);
  style.borderWidthIsUniform = uniformWidth;

  const bool uniformColor =
      border.borderColors.top == border.borderColors.right &&
      border.borderColors.right == border.borderColors.bottom &&
      border.borderColors.bottom == border.borderColors.left;
  if (uniformColor) {
    if (facebook::react::isColorMeaningful(border.borderColors.top)) {
      style.hasBorderColor = true;
      style.borderColor = packColor(border.borderColors.top);
      style.borderColorIsUniform = true;
    }
  } else {
    // Non-uniform border colours: still draw the top edge so something appears.
    if (facebook::react::isColorMeaningful(border.borderColors.top)) {
      style.hasBorderColor = true;
      style.borderColor = packColor(border.borderColors.top);
    }
    style.borderColorIsUniform = false;
  }

  for (const auto& op : viewProps->transform.operations) {
    switch (op.type) {
      case facebook::react::TransformOperationType::Translate:
        style.translateX += static_cast<float>(op.x.value);
        style.translateY += static_cast<float>(op.y.value);
        break;
      case facebook::react::TransformOperationType::Scale:
        style.scale *= static_cast<float>(op.x.value);
        break;
      case facebook::react::TransformOperationType::Rotate:
        style.rotation +=
            static_cast<float>(qRadiansToDegrees(op.z.value));
        break;
      default:
        break;
    }
  }

  // Preserve legacy dynamic metadata. The opt-in generic View bridge maps a
  // bounded subset onto real Qt Accessible attached properties; an OS
  // transport/consumer and RN accessibility actions are separate capabilities.
  // This is a complete Fabric props snapshot. Empty identifiers deliberately
  // remove old values, unlike an omitted field in a sparse animated update.
  style.testId = QString::fromStdString(viewProps->testId);
  style.nativeId = QString::fromStdString(viewProps->nativeId);
  style.pointerEventsEnabled = viewProps->pointerEvents != facebook::react::PointerEventsMode::None;
  style.accessible = viewProps->accessible;
  style.accessibilityLabel =
      QString::fromStdString(viewProps->accessibilityLabel);
  style.accessibilityRole =
      QString::fromStdString(viewProps->accessibilityRole);
  style.accessibilityHint =
      QString::fromStdString(viewProps->accessibilityHint);
  if (viewProps->accessibilityValue.text.has_value()) {
    style.accessibilityValue = QString::fromStdString(
        viewProps->accessibilityValue.text.value());
  }
  if (viewProps->accessibilityState.has_value()) {
    style.accessibilityStateDisabled =
        viewProps->accessibilityState->disabled;
    style.accessibilityStateSelected =
        viewProps->accessibilityState->selected;
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
    style.accessibilityStateCheckable = viewProps->accessibilityState->checked != facebook::react::AccessibilityState::None;
    style.accessibilityStateMixed = viewProps->accessibilityState->checked == facebook::react::AccessibilityState::Mixed;
#endif
    style.accessibilityStateChecked =
        viewProps->accessibilityState->checked ==
        facebook::react::AccessibilityState::Checked;
  }

  return style;
}

namespace {

std::uint32_t packColorFromDynamic(const folly::dynamic& value) {
  if (value.isInt()) {
    return static_cast<std::uint32_t>(value.asInt());
  }
  return 0;
}

ViewStyle viewStyleFromDynamic(const folly::dynamic& props) {
  ViewStyle style;

  if (props.count("opacity")) {
    style.opacity = static_cast<float>(props["opacity"].asDouble());
  }

  if (props.count("backgroundColor")) {
    const auto packed = packColorFromDynamic(props["backgroundColor"]);
    if (packed != 0u) {
      style.hasBackgroundColor = true;
      style.backgroundColor = packed;
    }
  }

  if (props.count("borderRadius")) {
    style.borderRadius = static_cast<float>(props["borderRadius"].asDouble());
  }

  if (props.count("borderWidth")) {
    style.borderWidth = static_cast<float>(props["borderWidth"].asDouble());
  }

  if (props.count("borderColor")) {
    const auto packed = packColorFromDynamic(props["borderColor"]);
    if (packed != 0u) {
      style.hasBorderColor = true;
      style.borderColor = packed;
    }
  }

  if (props.count("transform")) {
    for (const auto& op : props["transform"]) {
      if (!op.isObject()) {
        continue;
      }
      for (const auto& item : op.items()) {
        const auto& rawKey = item.first;
        const auto& rawValue = item.second;
        if (rawValue.isNull()) {
          continue;
        }
        const std::string key = rawKey.asString();
        if (key == "translateX") {
          style.translateX += static_cast<float>(rawValue.asDouble());
        } else if (key == "translateY") {
          style.translateY += static_cast<float>(rawValue.asDouble());
        } else if (key == "scale") {
          style.scale *= static_cast<float>(rawValue.asDouble());
        } else if (key == "rotate") {
          // React Native's native transform uses radians, Qt Quick uses degrees.
          style.rotation +=
              static_cast<float>(qRadiansToDegrees(rawValue.asDouble()));
        }
      }
    }
  }

  return style;
}

}  // namespace

LayoutFrame toLayoutFrame(const facebook::react::LayoutMetrics& metrics) {
  return LayoutFrame{
      .x = metrics.frame.origin.x,
      .y = metrics.frame.origin.y,
      .width = metrics.frame.size.width,
      .height = metrics.frame.size.height,
  };
}

ShadowViewMutationAdapter::ShadowViewMutationAdapter(QtQuickMountingLayer& layer)
    : layer_(layer) {}

void ShadowViewMutationAdapter::synchronouslyUpdateViewOnUIThread(
    facebook::react::Tag tag,
    const folly::dynamic& props) {
  layer_.updateStyle(tag, viewStyleFromDynamic(props));
}

void ShadowViewMutationAdapter::setTouchDispatcher(
    QtTouchDispatcher* dispatcher) {
  touchDispatcher_ = dispatcher;
}

void ShadowViewMutationAdapter::mapComponent(
    const QString& componentName,
    const QString& qmlTypeName) {
  componentTypes_.insert(componentName, qmlTypeName);
}

void ShadowViewMutationAdapter::applyModal(const ShadowView& view) {
  if (componentNameOf(view) != QStringLiteral("ModalHostView")) return;
  auto* item = layer_.itemForTag(view.tag);
  if (item == nullptr) return;
  auto& bridge = modalBridges_[view.tag];
  if (!bridge) bridge = new QtModalBridge(*item, layer_.rootItem());
  bridge->update(std::dynamic_pointer_cast<const QtModalBridge::State>(view.state),
      std::dynamic_pointer_cast<const facebook::react::ModalHostViewEventEmitter>(view.eventEmitter));
#if RNQNX_ENABLE_MODAL_FOCUS
  const auto emitter = std::dynamic_pointer_cast<const facebook::react::ModalHostViewEventEmitter>(view.eventEmitter);
  layer_.setModalRequestClose(view.tag, [emitter] { if (emitter) emitter->onRequestClose({}); });
#endif
}

void ShadowViewMutationAdapter::applyImage(const ShadowView& view) {
  const auto* props =
      dynamic_cast<const facebook::react::ImageProps*>(view.props.get());
  if (props == nullptr) {
    return;
  }

  QString uri;
  if (!props->sources.empty()) {
    uri = QString::fromStdString(props->sources.front().uri);
  } else if (!props->defaultSource.uri.empty()) {
    uri = QString::fromStdString(props->defaultSource.uri);
  }
  if (!uri.isEmpty()) {
    layer_.updateImageSource(view.tag, uri);
  }

  const QString loadingUri =
      props->loadingIndicatorSource.uri.empty()
          ? QString()
          : QString::fromStdString(props->loadingIndicatorSource.uri);
  layer_.updateImageLoadingSource(view.tag, loadingUri);

  // Map React Native's ImageResizeMode to Qt Quick's Image.fillMode.
  // Qt Quick values are: Stretch=0, PreserveAspectFit=1, PreserveAspectCrop=2.
  int fillMode = 0;
  switch (props->resizeMode) {
    case facebook::react::ImageResizeMode::Cover:
      fillMode = 2;
      break;
    case facebook::react::ImageResizeMode::Contain:
      fillMode = 1;
      break;
    case facebook::react::ImageResizeMode::Stretch:
    default:
      fillMode = 0;
      break;
  }
  layer_.updateImageFillMode(view.tag, fillMode);
}

void ShadowViewMutationAdapter::applyText(const ShadowView& view) {
  if (dynamic_cast<const facebook::react::ConcreteState<
          facebook::react::ParagraphState>*>(view.state.get()) == nullptr) return;
  const TextStyle style = paragraphTextStyle(view);
  layer_.updateText(view.tag, style);
}

MountError ShadowViewMutationAdapter::applyTextInput(const ShadowView& view) {
  const auto* props =
      dynamic_cast<const facebook::react::TextInputProps*>(view.props.get());
  if (props == nullptr) {
    return MountError::None;
  }

  TextInputStyle style;
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  style.mostRecentEventCount = props->mostRecentEventCount;
  if (props->selection) {
    style.selectionStart = props->selection->start;
    style.selectionEnd = props->selection->end;
  }
#endif
  style.text = QString::fromStdString(props->text);
  style.readOnly = props->readOnly || !props->editable;
  // Secure entry lives on the traits rather than the props themselves
  // (iostextinput/primitives.h:162, TextInputProps.h:29).
  style.secure = props->traits.secureTextEntry;
  if (props->maxLength != std::numeric_limits<int>::max()) {
    style.maxLength = props->maxLength;
  }

  // The field's own text attributes, not a paragraph's: a TextInput has one
  // style for its whole content.
  // RN 0.88 records the surface scale in LayoutMetrics even when an empty
  // input has no attributed fragments/state update. Do not infer it from text.
  const auto multiplier = view.layoutMetrics.fontSizeMultiplier;
  const float fontSizeMultiplier = std::isfinite(multiplier) && multiplier > 0
      ? multiplier : 1.0F;
  const auto attributes = props->getEffectiveTextAttributes(fontSizeMultiplier);
  style.font = rnqnx::text::fontFromTextAttributes(attributes);
  const auto& insets = view.layoutMetrics.contentInsets;
  style.contentInsets = QMarginsF(insets.left, insets.top, insets.right, insets.bottom);
  if (attributes.foregroundColor) {
    style.hasColor = true;
    style.color = static_cast<std::uint32_t>(*attributes.foregroundColor);
  }

  style.placeholder = QString::fromStdString(props->placeholder);
  style.multiline = props->multiline;
  if (facebook::react::isColorMeaningful(props->placeholderTextColor)) {
    style.hasPlaceholderTextColor = true;
    style.placeholderTextColor =
        static_cast<std::uint32_t>(*props->placeholderTextColor);
  }

  return layer_.updateTextInput(view.tag, style);
}

void ShadowViewMutationAdapter::applyActivityIndicator(const ShadowView& view) {
  if (componentNameOf(view) != QStringLiteral("ActivityIndicatorView")) {
    return;
  }
  const auto* props =
      dynamic_cast<const facebook::react::ActivityIndicatorViewProps*>(
          view.props.get());
  if (props == nullptr) {
    return;
  }

  ActivityIndicatorStyle style;
  style.animating = props->animating;
  style.hidesWhenStopped = props->hidesWhenStopped;
  if (facebook::react::isColorMeaningful(props->color)) {
    style.hasColor = true;
    style.color = packColor(props->color);
  }
  style.size =
      props->size == facebook::react::ActivityIndicatorViewSize::Large
          ? QStringLiteral("large")
          : QStringLiteral("small");

  layer_.updateActivityIndicator(view.tag, style);
}

void ShadowViewMutationAdapter::applySwitch(const ShadowView& view) {
  if (componentNameOf(view) != QStringLiteral("Switch")) {
    return;
  }
  const auto* props =
      dynamic_cast<const facebook::react::SwitchProps*>(view.props.get());
  if (props == nullptr) {
    return;
  }

  SwitchStyle style;
  style.value = props->value;
  style.disabled = props->disabled;
  if (facebook::react::isColorMeaningful(props->onTintColor)) {
    style.hasTrackOnColor = true;
    style.trackOnColor = packColor(props->onTintColor);
  }
  if (facebook::react::isColorMeaningful(props->tintColor)) {
    style.hasTrackOffColor = true;
    style.trackOffColor = packColor(props->tintColor);
  }
  if (facebook::react::isColorMeaningful(props->thumbTintColor)) {
    style.hasThumbColor = true;
    style.thumbColor = packColor(props->thumbTintColor);
  }

  if (auto bridge = switchBridges_.value(view.tag)) {
    bridge->setExpectedValue(style.value);
  }

  layer_.updateSwitch(view.tag, style);
}

void ShadowViewMutationAdapter::connectSwitchEvents(const ShadowView& view) {
  if (componentNameOf(view) != QStringLiteral("Switch")) return;
  const auto emitter =
      std::dynamic_pointer_cast<const facebook::react::SwitchEventEmitter>(
          view.eventEmitter);
  QQuickItem* item = layer_.itemForTag(view.tag);
  if (item == nullptr) {
    return;
  }

  const auto* props =
      dynamic_cast<const facebook::react::SwitchProps*>(view.props.get());
  const bool initialValue = props != nullptr ? props->value : false;

  const auto emitChange = [emitter, viewTag = view.tag](bool value) {
    if (!emitter) return;
    facebook::react::SwitchEventEmitter::OnChange event;
    event.value = value;
    event.target = static_cast<int>(viewTag);
    emitter->onChange(event);
  };

  if (auto bridge = switchBridges_.value(view.tag)) {
    bridge->setOnChange(emitChange);
    return;
  }

  auto* bridge =
      new QtSwitchBridge(emitChange, item, initialValue, item);
  QObject::connect(
      item, SIGNAL(toggled()), bridge, SLOT(notify()));

  switchBridges_.insert(view.tag, bridge);
}

void ShadowViewMutationAdapter::connectImageEvents(const ShadowView& view) {
  if (imageBridges_.contains(view.tag)) {
    return;
  }

  const auto emitter =
      std::dynamic_pointer_cast<const facebook::react::ImageEventEmitter>(
          view.eventEmitter);
  if (emitter == nullptr) {
    return;
  }

  QQuickItem* item = layer_.itemForTag(view.tag);
  if (item == nullptr) {
    return;
  }

  const auto* props =
      dynamic_cast<const facebook::react::ImageProps*>(view.props.get());
  facebook::react::ImageSource source;
  if (props != nullptr && !props->sources.empty()) {
    source = props->sources.front();
  }

  auto* bridge = new QtImageBridge(
      item,
      std::move(source),
      [emitter]() { emitter->onLoadStart(); },
      [emitter](const facebook::react::ImageSource& s) {
        emitter->onLoad(s);
      },
      [emitter]() { emitter->onLoadEnd(); },
      [emitter](const facebook::react::ImageErrorInfo& e) {
        emitter->onError(e);
      },
      item);
  item->setProperty("bridge", QVariant::fromValue<QObject*>(bridge));

  imageBridges_.insert(view.tag, bridge);
}

namespace {
facebook::react::TextInputEventEmitter::Metrics makeTextInputMetrics(
    QQuickItem* item,
    facebook::react::Tag tag,
    int eventCount) {
  facebook::react::TextInputEventEmitter::Metrics metrics{};
  metrics.text = item->property("text").toString().toStdString();
  metrics.target = tag;
  metrics.eventCount = eventCount;
  metrics.containerSize = {
      .width = static_cast<facebook::react::Float>(item->width()),
      .height = static_cast<facebook::react::Float>(item->height())};
  metrics.layoutMeasurement = metrics.containerSize;
  metrics.zoomScale = 1.0F;
  return metrics;
}
}  // namespace

void ShadowViewMutationAdapter::connectTextInputEvents(const ShadowView& view) {
  if (textInputConnected_.contains(view.tag)) {
    return;
  }

  const auto emitter =
      std::dynamic_pointer_cast<const facebook::react::TextInputEventEmitter>(
          view.eventEmitter);
  if (emitter == nullptr) {
    return;
  }

  const facebook::react::Tag tag = view.tag;
  QQuickItem* item = layer_.itemForTag(tag);
  // A plain Item has no `text` property, so this also rejects a TextInput
  // component mapped onto a QML type that cannot edit.
  if (item == nullptr || !item->property("text").isValid()) {
    return;
  }

#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  auto* bridge = layer_.textInputBridge(tag);
  if (bridge == nullptr) return;
  bridge->setEmitter([emitter, item, tag](TextInputEvent kind, const TextInputObservation& value) {
    auto metrics = makeTextInputMetrics(item, tag, value.eventCount);
    metrics.text = value.text.toStdString();
    metrics.selectionRange = {.location = value.selectionStart, .length = value.selectionEnd - value.selectionStart};
    switch (kind) {
      case TextInputEvent::Change: emitter->onChange(metrics); break;
      case TextInputEvent::Selection: emitter->onSelectionChange(metrics); break;
      case TextInputEvent::Focus: emitter->onFocus(metrics); break;
      case TextInputEvent::Blur: emitter->onBlur(metrics); break;
      case TextInputEvent::Submit: emitter->onSubmitEditing(metrics); break;
      case TextInputEvent::Key: {
        facebook::react::TextInputEventEmitter::KeyPressMetrics key{};
        key.text = value.key.toStdString();
        key.eventCount = value.eventCount;
        emitter->onKeyPress(key);
        break;
      }
    }
  });
#else
  // eventCount is React Native's ordering token: JavaScript compares it against
  // the count it last sent so it can drop an update that crossed a keystroke in
  // flight. It has to increase per emission, so it is owned here.
  auto changeCount = std::make_shared<int>(0);
  const auto emitChange = [emitter, item, tag, changeCount]() {
    auto metrics = makeTextInputMetrics(item, tag, ++(*changeCount));
    emitter->onChange(metrics);
  };

  auto focusCount = std::make_shared<int>(0);

  auto keyCount = std::make_shared<int>(0);
  const auto emitKeyPress = [emitter, item, keyCount]() {
    facebook::react::TextInputEventEmitter::KeyPressMetrics metrics{};
    metrics.text = item->property("lastKeyText").toString().toStdString();
    metrics.eventCount = ++(*keyCount);
    emitter->onKeyPress(metrics);
  };

  auto submitCount = std::make_shared<int>(0);
  const auto emitSubmit = [emitter, item, tag, submitCount]() {
    auto metrics = makeTextInputMetrics(item, tag, ++(*submitCount));
    emitter->onSubmitEditing(metrics);
  };

  auto selectionCount = std::make_shared<int>(0);
  const auto emitSelection = [emitter, item, tag, selectionCount]() {
    auto metrics = makeTextInputMetrics(item, tag, ++(*selectionCount));
    const int start = item->property("selectionStart").toInt();
    const int end = item->property("selectionEnd").toInt();
    metrics.selectionRange = {
        .location = start,
        .length = end - start,
    };
    emitter->onSelectionChange(metrics);
  };

  // The bridge is parented to the item, so it dies with it.
  auto* changeBridge = new QtScrollBridge(emitChange, item);
  QObject::connect(item, SIGNAL(textChanged()), changeBridge, SLOT(notify()));

  QObject::connect(
      item, &QQuickItem::activeFocusChanged, [emitter, item, tag, focusCount](bool) {
        auto metrics = makeTextInputMetrics(item, tag, ++(*focusCount));
        if (item->hasActiveFocus()) {
          emitter->onFocus(metrics);
        } else {
          emitter->onBlur(metrics);
        }
      });

  auto* keyBridge = new QtScrollBridge(emitKeyPress, item);
  QObject::connect(item, SIGNAL(keyPressed()), keyBridge, SLOT(notify()));

  auto* submitBridge = new QtScrollBridge(emitSubmit, item);
  QObject::connect(item, SIGNAL(submitEditing()), submitBridge, SLOT(notify()));

  auto* selectionBridge = new QtScrollBridge(emitSelection, item);
  QObject::connect(
      item, SIGNAL(selectionChanged()), selectionBridge, SLOT(notify()));

#endif
  textInputConnected_.insert(view.tag);
}

void ShadowViewMutationAdapter::connectScrollEvents(const ShadowView& view) {
  if (scrollConnected_.contains(view.tag)) {
    return;
  }

  const auto emitter =
      std::dynamic_pointer_cast<const facebook::react::ScrollViewEventEmitter>(
          view.eventEmitter);
  if (emitter == nullptr) {
    return;
  }

  QQuickItem* item = layer_.itemForTag(view.tag);
  if (item == nullptr ||
      item->property("contentItem").value<QQuickItem*>() == nullptr) {
    return;
  }

  const float zoomScale = [view]() -> float {
    const auto* scrollProps =
        dynamic_cast<const facebook::react::BaseScrollViewProps*>(
            view.props.get());
    return scrollProps != nullptr
        ? static_cast<float>(scrollProps->zoomScale)
        : 1.0F;
  }();

  // Qt emits contentXChanged/contentYChanged as the Flickable moves. The item
  // is the connection context, so the handler dies with it.
  const auto makeScrollEvent = [item, zoomScale]() {
    facebook::react::ScrollEvent event;
    event.contentOffset = {
        .x = static_cast<facebook::react::Float>(
            item->property("contentX").toDouble()),
        .y = static_cast<facebook::react::Float>(
            item->property("contentY").toDouble())};
    event.contentSize = {
        .width = static_cast<facebook::react::Float>(
            item->property("contentWidth").toDouble()),
        .height = static_cast<facebook::react::Float>(
            item->property("contentHeight").toDouble())};
    event.containerSize = {
        .width = static_cast<facebook::react::Float>(item->width()),
        .height = static_cast<facebook::react::Float>(item->height())};
    event.zoomScale = zoomScale;
    return event;
  };

  const auto emitScroll = [emitter, makeScrollEvent]() {
    emitter->onScroll(makeScrollEvent());
  };

  const auto emitScrollBeginDrag = [emitter, makeScrollEvent]() {
    emitter->onScrollBeginDrag(makeScrollEvent());
  };

  const auto emitMomentumScrollBegin = [emitter, makeScrollEvent]() {
    emitter->onMomentumScrollBegin(makeScrollEvent());
  };

  const auto emitMomentumScrollEnd = [emitter, makeScrollEvent]() {
    emitter->onMomentumScrollEnd(makeScrollEvent());
  };

  const auto emitScrollEndDrag = [emitter, item, makeScrollEvent]() {
    facebook::react::ScrollEndDragEvent event{makeScrollEvent()};
    event.velocity = {
        .x = static_cast<facebook::react::Float>(
            item->property("horizontalVelocity").toDouble()),
        .y = static_cast<facebook::react::Float>(
            item->property("verticalVelocity").toDouble())};
    event.targetContentOffset = event.contentOffset;
    emitter->onScrollEndDrag(event);
  };

  // The bridge is parented to the item, so it dies with it.
  auto* scrollBridge = new QtScrollBridge(emitScroll, item);
  QObject::connect(
      item, SIGNAL(contentXChanged()), scrollBridge, SLOT(notify()));
  QObject::connect(
      item, SIGNAL(contentYChanged()), scrollBridge, SLOT(notify()));

  auto* beginDragBridge = new QtScrollBridge(emitScrollBeginDrag, item);
  QObject::connect(
      item, SIGNAL(dragStarted()), beginDragBridge, SLOT(notify()));

  auto* endDragBridge = new QtScrollBridge(emitScrollEndDrag, item);
  QObject::connect(item, SIGNAL(dragEnded()), endDragBridge, SLOT(notify()));

  auto* momentumBeginBridge =
      new QtScrollBridge(emitMomentumScrollBegin, item);
  QObject::connect(
      item, SIGNAL(flickStarted()), momentumBeginBridge, SLOT(notify()));

  auto* momentumEndBridge = new QtScrollBridge(emitMomentumScrollEnd, item);
  QObject::connect(
      item, SIGNAL(flickEnded()), momentumEndBridge, SLOT(notify()));

  scrollConnected_.insert(view.tag);
}

ShadowViewMutationAdapter::~ShadowViewMutationAdapter() {
  for (const auto& bridge : std::as_const(viewFocusBridges_)) {
    if (bridge) {
      bridge->setMounted(false);
      delete bridge.data();
    }
  }
#if RNQNX_ENABLE_MODAL_FOCUS
  for (auto it = modalBridges_.cbegin(); it != modalBridges_.cend(); ++it) layer_.setModalRequestClose(it.key(), {});
#endif
  for (const auto& bridge : std::as_const(modalBridges_)) {
    if (bridge) {
      bridge->setMounted(false, QtModalBridge::FocusRestoration::Skip);
      delete bridge.data();
    }
  }
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
  for (const auto& receiver : std::as_const(accessibilityActivations_)) {
    if (receiver) { receiver->invalidate(); delete receiver.data(); }
  }
#endif
}
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
MountError ShadowViewMutationAdapter::applyAccessibilityActions(const ShadowView& view, QString& detail) {
  // Replace the receiver on every props/emitter update. QObject destruction also
  // cancels its posted calls; an old generation cannot invoke a new emitter.
  if (auto previous = accessibilityActivations_.take(view.tag)) {
    previous->invalidate(); delete previous.data();
  }
  if (!layer_.isGenericView(view.tag)) return MountError::None;
  auto* item = layer_.itemForTag(view.tag);
  const auto props = std::dynamic_pointer_cast<const facebook::react::ViewProps>(view.props);
  if (!item || !props) return MountError::None;
  bool registered = false;
  for (const auto& action : props->accessibilityActions) {
    if (action.name != "activate" || registered) {
      detail = QStringLiteral("Only one registered accessibility activate action is supported");
      return MountError::UnsupportedAccessibility;
    }
    registered = true;
  }
  if (!registered) return MountError::None;
  const auto emitter = std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(view.eventEmitter);
  AccessibilityActivationPolicy policy{props->accessible, registered, props->onAccessibilityAction && bool(emitter),
    props->accessibilityState.has_value() && props->accessibilityState->disabled};
  auto* receiver = new QtAccessibilityActivation(*item, policy, [emitter] {
    if (emitter) emitter->onAccessibilityAction("activate");
  });
  if (!receiver->connectPress(detail)) {
    delete receiver;
    return MountError::UnsupportedAccessibility;
  }
  accessibilityActivations_.insert(view.tag, receiver);
  return MountError::None;
}
#endif

void ShadowViewMutationAdapter::registerEventEmitter(const ShadowView& view) {
  if (touchDispatcher_ == nullptr) {
    return;
  }
  // ShadowView::eventEmitter (ShadowView.h:45) is the shadow node's emitter.
  // Only view emitters carry touches; anything else simply does not register.
  touchDispatcher_->setEventEmitter(
      view.tag,
      std::dynamic_pointer_cast<const facebook::react::TouchEventEmitter>(
          view.eventEmitter));
}

void ShadowViewMutationAdapter::connectViewFocusEvents(const ShadowView& view) {
  // TextInput has richer focus payloads and its own bridge. Connecting every
  // BaseViewEventEmitter here would duplicate those native focus events.
  if (componentNameOf(view) != QStringLiteral("View")) return;
  auto* item = layer_.itemForTag(view.tag);
  if (item == nullptr) return;
  auto& bridge = viewFocusBridges_[view.tag];
  if (!bridge) bridge = new QtViewFocusBridge(*item);
  const auto emitter =
      std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(view.eventEmitter);
  const auto props = std::dynamic_pointer_cast<const facebook::react::ViewProps>(view.props);
  const bool button = props &&
      (props->role == facebook::react::Role::Button ||
       (props->role == facebook::react::Role::None && props->accessibilityRole == "button"));
  const bool disabled = props && props->accessibilityState && props->accessibilityState->disabled;
  bridge->update([emitter](bool focused) {
    if (!emitter) return;
    if (focused) emitter->onFocus();
    else emitter->onBlur();
  }, [emitter] {
    // Pressability accepts non-pointer clicks as keyboard activation. Its
    // PointerEvent onClick path contains pointerType and is deliberately
    // ignored by Pressability to avoid a duplicate touch-generated press.
    if (emitter) emitter->dispatchEvent("click", folly::dynamic::object());
  }, button && !disabled);
}

MountError ShadowViewMutationAdapter::applyOne(
    const ShadowViewMutation& mutation,
    QString& detail) {
  switch (mutation.type) {
    case ShadowViewMutation::Type::Create: {
      const ShadowView& view = mutation.newChildShadowView;
      const QString componentName = componentNameOf(view);
      const auto qmlType = componentTypes_.constFind(componentName);
      if (qmlType == componentTypes_.constEnd()) {
        detail = QStringLiteral("No QML type mapped for component '%1'")
                     .arg(componentName);
        return MountError::UnknownComponentType;
      }

      const MountError error = layer_.create(
          view.tag, *qmlType, componentName == QStringLiteral("ModalHostView"));
      if (error != MountError::None) {
        detail = layer_.lastError();
        return error;
      }

      // Create carries the first layout and props, so applying both here avoids
      // a frame at the origin with zero size and no colour before the first
      // Update arrives.
      const MountError layoutError =
          layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
      if (layoutError != MountError::None) {
        return layoutError;
      }
      if (auto viewEmitter =
              std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(
                  view.eventEmitter)) {
        viewEmitter->onLayout(view.layoutMetrics);
      }
      registerEventEmitter(view);
      connectViewFocusEvents(view);
      connectScrollEvents(view);
      connectTextInputEvents(view);
      connectSwitchEvents(view);
      connectImageEvents(view);
      applyText(view);
      applyImage(view);
      const auto textInputError = applyTextInput(view);
      if (textInputError != MountError::None) { detail = layer_.lastError(); return textInputError; }
      applyActivityIndicator(view);
      applySwitch(view);
      applyModal(view);
      const auto styleError = layer_.updateStyle(view.tag, toViewStyle(view));
      if (styleError != MountError::None) return styleError;
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
      return applyAccessibilityActions(view, detail);
#else
      return MountError::None;
#endif
    }

    case ShadowViewMutation::Type::Delete:
      if (auto previous = viewFocusBridges_.take(mutation.oldChildShadowView.tag)) {
        previous->setMounted(false);
        delete previous.data();
      }
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
      if (auto previous = accessibilityActivations_.take(mutation.oldChildShadowView.tag)) {
        previous->invalidate(); delete previous.data();
      }
#endif
      if (touchDispatcher_ != nullptr) {
        touchDispatcher_->forgetTag(mutation.oldChildShadowView.tag);
      }
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
      textInputConnected_.remove(mutation.oldChildShadowView.tag);
#endif
      switchBridges_.remove(mutation.oldChildShadowView.tag);
      imageBridges_.remove(mutation.oldChildShadowView.tag);
      if (auto previous = modalBridges_.take(mutation.oldChildShadowView.tag)) {
        previous->setMounted(false);
        delete previous.data();
      }
      return layer_.destroy(mutation.oldChildShadowView.tag);

    case ShadowViewMutation::Type::Insert: {
      const MountError error = layer_.insert(
          mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);
      if (error != MountError::None) {
        return error;
      }
      if (auto bridge = viewFocusBridges_.value(mutation.newChildShadowView.tag)) {
        bridge->setMounted(true);
      }
      if (auto bridge = modalBridges_.value(mutation.newChildShadowView.tag)) {
        bridge->setMounted(true);
      }
      // Qt's Flickable does not derive its content size from its children, so
      // it is recomputed whenever they change.
      return layer_.updateScrollContentSize(mutation.parentTag);
    }

    case ShadowViewMutation::Type::Remove: {
      const auto bridge = viewFocusBridges_.value(mutation.oldChildShadowView.tag);
      const auto modal = modalBridges_.value(mutation.oldChildShadowView.tag);
      if (bridge) bridge->setMounted(false);
      if (modal) modal->setMounted(false);
      const MountError error = layer_.remove(
          mutation.parentTag, mutation.oldChildShadowView.tag, mutation.index);
      if (error != MountError::None) {
        if (bridge) bridge->setMounted(true);
        if (modal) modal->setMounted(true);
        return error;
      }
      return layer_.updateScrollContentSize(mutation.parentTag);
    }

    case ShadowViewMutation::Type::Update: {
      const ShadowView& view = mutation.newChildShadowView;
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
      // Close the old admission before any property change can notify Qt or
      // fail validation. A failed update must not leave a stale action active.
      if (auto previous = accessibilityActivations_.take(view.tag)) {
        previous->invalidate(); delete previous.data();
      }
#endif
      registerEventEmitter(view);
      connectViewFocusEvents(view);
      const MountError layoutError =
          layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
      if (layoutError != MountError::None) {
        return layoutError;
      }
      if (auto viewEmitter =
              std::dynamic_pointer_cast<const facebook::react::ViewEventEmitter>(
                  view.eventEmitter)) {
        viewEmitter->onLayout(view.layoutMetrics);
      }
      connectTextInputEvents(view);
      connectSwitchEvents(view);
      connectImageEvents(view);
      applyText(view);
      applyImage(view);
      const auto textInputError = applyTextInput(view);
      if (textInputError != MountError::None) { detail = layer_.lastError(); return textInputError; }
      applyActivityIndicator(view);
      applySwitch(view);
      applyModal(view);
      const MountError styleError = layer_.updateStyle(view.tag, toViewStyle(view));
      if (styleError != MountError::None) {
        return styleError;
      }
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
      const auto actionError = applyAccessibilityActions(view, detail);
      if (actionError != MountError::None) return actionError;
#endif
      // A child resizing changes what its scrollable parent has to cover.
      if (mutation.parentTag >= 0) {
        layer_.updateScrollContentSize(mutation.parentTag);
      }
      return MountError::None;
    }
  }

  detail = QStringLiteral("Unhandled mutation type %1")
               .arg(static_cast<int>(mutation.type));
  return MountError::UnknownTag;
}

std::vector<MutationFailure> ShadowViewMutationAdapter::apply(
    const ShadowViewMutation::List& mutations) {
  std::vector<MutationFailure> failures;

  for (const ShadowViewMutation& mutation : mutations) {
    QString detail;
    const MountError error = applyOne(mutation, detail);
    if (error == MountError::None) {
      continue;
    }

    const Tag tag = mutation.type == ShadowViewMutation::Type::Delete ||
            mutation.type == ShadowViewMutation::Type::Remove
        ? mutation.oldChildShadowView.tag
        : mutation.newChildShadowView.tag;

    failures.push_back(MutationFailure{
        .type = mutation.type,
        .tag = tag,
        .error = error,
        .detail = detail.isEmpty() ? layer_.lastError() : detail,
    });
  }

  return failures;
}

MutationFailure ShadowViewMutationAdapter::dispatchCommand(
    facebook::react::Tag tag,
    const std::string& commandName,
    const folly::dynamic& args,
    const ShadowView* shadowView) {
  if (commandName == "setValue") {
    if (!args.isArray() || args.size() != 1 || !args[0].isBool()) {
      return MutationFailure{.type = ShadowViewMutation::Type::Update, .tag = tag,
          .error = MountError::IndexOutOfRange,
          .detail = QStringLiteral("Switch setValue expects [boolean]")};
    }
    const auto bridge = switchBridges_.value(tag);
    if (!bridge || !layer_.itemForTag(tag)) {
      return MutationFailure{.type = ShadowViewMutation::Type::Update, .tag = tag,
          .error = MountError::UnknownTag,
          .detail = QStringLiteral("Switch setValue requires a mounted Switch event bridge")};
    }
    bridge->setValue(args[0].asBool());
    return MutationFailure{};
  }
  if (commandName == "scrollTo") {
    if (!args.isArray() || args.size() < 2) {
      return MutationFailure{
          .type = ShadowViewMutation::Type::Update,
          .tag = tag,
          .error = MountError::UnknownTag,
          .detail = QStringLiteral("scrollTo expects [x, y]")};
    }
    const double x = args[0].asDouble();
    const double y = args[1].asDouble();
    const MountError error = layer_.scrollTo(tag, x, y, false);
    if (error != MountError::None) {
      return MutationFailure{
          .type = ShadowViewMutation::Type::Update,
          .tag = tag,
          .error = error,
          .detail = layer_.lastError()};
    }
    return MutationFailure{};
  }

#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
  if (commandName == "setTextAndSelection") {
    const auto validInt = [](const folly::dynamic& value) {
      return value.isNumber() && std::isfinite(value.asDouble()) &&
          std::trunc(value.asDouble()) == value.asDouble() &&
          value.asDouble() >= std::numeric_limits<int>::min() && value.asDouble() <= std::numeric_limits<int>::max();
    };
    if (!args.isArray() || args.size() != 4 || !validInt(args[0]) ||
        !(args[1].isNull() || args[1].isString()) || !validInt(args[2]) || !validInt(args[3])) {
      return MutationFailure{.type = ShadowViewMutation::Type::Update, .tag = tag,
          .error = MountError::IndexOutOfRange, .detail = QStringLiteral("Invalid TextInput command arguments")};
    }
    const std::optional<QString> text = args[1].isNull() ? std::nullopt :
        std::optional<QString>(QString::fromStdString(args[1].asString()));
    const auto error = layer_.setTextAndSelection(tag, static_cast<int>(args[0].asDouble()), text,
                                                 static_cast<int>(args[2].asDouble()), static_cast<int>(args[3].asDouble()));
    return MutationFailure{.type = ShadowViewMutation::Type::Update, .tag = tag,
        .error = error, .detail = error == MountError::None ? QString() : layer_.lastError()};
  }
#endif

  if (commandName == "focus") {
    const MountError error = layer_.focusTextInput(tag);
    if (error != MountError::None) {
      return MutationFailure{
          .type = ShadowViewMutation::Type::Update,
          .tag = tag,
          .error = error,
          .detail = layer_.lastError()};
    }
    return MutationFailure{};
  }

  if (commandName == "blur") {
    const MountError error = layer_.blurTextInput(tag);
    if (error != MountError::None) {
      return MutationFailure{
          .type = ShadowViewMutation::Type::Update,
          .tag = tag,
          .error = error,
          .detail = layer_.lastError()};
    }
    return MutationFailure{};
  }

  if (commandName == "scrollToEnd") {
    bool horizontal = false;
    if (shadowView != nullptr) {
      const auto* scrollProps =
          dynamic_cast<const facebook::react::BaseScrollViewProps*>(
              shadowView->props.get());
      if (scrollProps != nullptr) {
        horizontal = scrollProps->horizontal;
      }
    }
    const bool animated =
        args.isArray() && !args.empty() ? args[0].asBool() : false;
    const MountError error = layer_.scrollToEnd(tag, animated, horizontal);
    if (error != MountError::None) {
      return MutationFailure{
          .type = ShadowViewMutation::Type::Update,
          .tag = tag,
          .error = error,
          .detail = layer_.lastError()};
    }
    return MutationFailure{};
  }

  return MutationFailure{
      .type = ShadowViewMutation::Type::Update,
      .tag = tag,
      .error = MountError::UnknownTag,
      .detail = QStringLiteral("Unknown command '%1'")
                    .arg(QString::fromStdString(commandName))};
}

}  // namespace rnqnx::fabric
