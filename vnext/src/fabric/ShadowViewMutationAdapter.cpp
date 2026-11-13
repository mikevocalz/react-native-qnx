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

#include "react-native-qnx/fabric/QtExtraComponents.h"
#if RNQNX_ENABLE_QT_RNSVG
#include <RnsvgItems.h>
#endif
#if RNQNX_ENABLE_QT_SVG
#include "react-native-qnx/fabric/QtSvgBridge.h"
#include <QtSvgItem.h>
#include <react/renderer/components/RnqnxQtSvg/EventEmitters.h>
#include <react/renderer/components/RnqnxQtSvg/Props.h>
#endif

#include <react/renderer/components/iostextinput/TextInputProps.h>
#include <react/renderer/components/textinput/TextInputEventEmitter.h>

#include <QColor>
#include <QMetaMethod>
#include <QQuickItem>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/components/FBReactNativeSpec/EventEmitters.h>
#include <react/renderer/components/FBReactNativeSpec/Props.h>
#include <react/renderer/components/image/ImageEventEmitter.h>
#include <react/renderer/components/modal/ModalHostViewShadowNode.h>
#include <react/renderer/components/scrollview/BaseScrollViewProps.h>
#include <react/renderer/components/image/ImageProps.h>
#include <react/renderer/components/scrollview/ScrollViewEventEmitter.h>
#include <react/renderer/components/view/ViewEventEmitter.h>
#include <react/renderer/components/text/BaseParagraphProps.h>
#include <react/renderer/components/text/ParagraphState.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/ConcreteState.h>
#include <react/renderer/graphics/Color.h>
#include <react/renderer/mounting/ShadowView.h>

#include "react-native-qnx/fabric/ViewObserver.h"

namespace rnqnx::fabric {

namespace {
ViewObserver& observerStorage() {
  static ViewObserver observer;
  return observer;
}
}  // namespace

void setViewObserver(ViewObserver observer) { observerStorage() = std::move(observer); }
const ViewObserver& viewObserver() { return observerStorage(); }

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

/// Flattens a paragraph's attributed string into plain text.
///
/// Per-fragment attributes — font, colour, weight — are measured by the text
/// layout manager but not yet carried onto the Qt item, so a paragraph renders
/// with one style. Recorded in docs/component-support.md.
/// CSS for one fragment, in the subset Qt's rich text understands.
QString fragmentStyleAttribute(const facebook::react::TextAttributes& attributes) {
  QStringList parts;
  if (!std::isnan(attributes.fontSize) && attributes.fontSize > 0) {
    parts << QStringLiteral("font-size:%1px").arg(static_cast<int>(attributes.fontSize));
  }
  if (facebook::react::isColorMeaningful(attributes.foregroundColor)) {
    parts << QStringLiteral("color:%1").arg(
        QColor::fromRgba(packColor(attributes.foregroundColor)).name(QColor::HexRgb));
  }
  if (attributes.fontWeight.has_value()) {
    parts << QStringLiteral("font-weight:%1")
                 .arg(static_cast<int>(*attributes.fontWeight));
  }
  if (attributes.fontStyle.has_value() &&
      *attributes.fontStyle == facebook::react::FontStyle::Italic) {
    parts << QStringLiteral("font-style:italic");
  }
  if (!attributes.fontFamily.empty()) {
    parts << QStringLiteral("font-family:'%1'").arg(
        QString::fromStdString(attributes.fontFamily));
  }
  return parts.join(QLatin1Char(';'));
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

  bool first = true;
  int fragmentCount = 0;
  for (const auto& fragment : state->getData().attributedString.getFragments()) {
    if (fragment.isAttachment()) {
      continue;
    }
    ++fragmentCount;
    style.text += QString::fromStdString(fragment.string);

    // Per-fragment styling is carried as rich text, because Qt Quick's Text
    // applies its own font and colour to the whole item. The plain string is
    // kept alongside it for anything that reads text without markup.
    style.richText += QStringLiteral("<span style=\"%1\">%2</span>")
                          .arg(fragmentStyleAttribute(fragment.textAttributes))
                          .arg(QString::fromStdString(fragment.string).toHtmlEscaped());

    if (first) {
      first = false;
      if (!std::isnan(fragment.textAttributes.fontSize)) {
        style.fontSize = static_cast<float>(fragment.textAttributes.fontSize);
      }
      if (facebook::react::isColorMeaningful(
              fragment.textAttributes.foregroundColor)) {
        style.hasColor = true;
        style.color = packColor(fragment.textAttributes.foregroundColor);
      }
      if (!fragment.textAttributes.fontFamily.empty()) {
        style.fontFamily =
            QString::fromStdString(fragment.textAttributes.fontFamily);
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
    : layer_(layer) {
#if RNQNX_ENABLE_QT_SVG
  mapComponent(QStringLiteral("QnxSvgDocument"), QStringLiteral("QnxSvgDocument"));
#endif
  for (const auto& name : extraComponentNames()) {
    const auto qtName = QString::fromStdString(name);
    mapComponent(qtName, qtName);
  }
}

#if RNQNX_ENABLE_QT_SVG
void ShadowViewMutationAdapter::applySvgDocument(const ShadowView& view) {
  const auto* props = dynamic_cast<const facebook::react::QnxSvgDocumentProps*>(view.props.get());
  if (!props) return;
  auto* item = qobject_cast<views::QtSvgItem*>(layer_.itemForTag(view.tag));
  if (!item) return;
  auto bridge = svgBridges_.value(view.tag);
  if (!bridge) {
    bridge = new QtSvgBridge(*item);
    svgBridges_.insert(view.tag, bridge);
  }
  bridge->apply(*props, std::dynamic_pointer_cast<const facebook::react::QnxSvgDocumentEventEmitter>(view.eventEmitter));
}
#endif

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
  const TextStyle style = paragraphTextStyle(view);
  if (!style.text.isEmpty()) {
    layer_.updateText(view.tag, style);
  }
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
  const auto attributes = props->getEffectiveTextAttributes(1.0F);
  if (!std::isnan(attributes.fontSize) && attributes.fontSize > 0) {
    style.fontSize = static_cast<float>(attributes.fontSize);
  }
  if (attributes.foregroundColor) {
    style.hasColor = true;
    style.color = static_cast<std::uint32_t>(*attributes.foregroundColor);
  }
  if (!attributes.fontFamily.empty()) {
    style.fontFamily = QString::fromStdString(attributes.fontFamily);
  }

  style.placeholder = QString::fromStdString(props->placeholder);
  style.multiline = props->multiline;
  // Yoga already subtracted padding and border from the content box; the
  // field draws its text there, as RCTTextInput and ReactEditText do.
  const auto& insets = view.layoutMetrics.contentInsets;
  style.insetLeft = static_cast<float>(insets.left);
  style.insetTop = static_cast<float>(insets.top);
  style.insetRight = static_cast<float>(insets.right);
  style.insetBottom = static_cast<float>(insets.bottom);
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

  if (auto* bridge = switchBridges_.value(view.tag, nullptr)) {
    bridge->setExpectedValue(style.value);
  }

  layer_.updateSwitch(view.tag, style);
}

MountError ShadowViewMutationAdapter::applyModal(const ShadowView& view) {
  if (componentNameOf(view) != QStringLiteral("ModalHostView")) {
    return MountError::None;
  }
  const MountError error = layer_.markOverlay(view.tag);
  if (error != MountError::None) {
    return error;
  }
  modalViews_.insert(view.tag, view);
  // The shadow node takes its size from this state. The portable C++ default
  // is 0x0, which laid the modal and all of its content out as nothing; the
  // surface root's size is the screen a QNX modal covers.
  const QSizeF size = layer_.rootSize();
  using ModalState =
      facebook::react::ConcreteState<facebook::react::ModalHostViewState>;
  if (const auto state = std::dynamic_pointer_cast<const ModalState>(view.state);
      state != nullptr && size.width() > 0 && size.height() > 0) {
    const auto& current = state->getData().screenSize;
    const auto width = static_cast<facebook::react::Float>(size.width());
    const auto height = static_cast<facebook::react::Float>(size.height());
    if (current.width != width || current.height != height) {
      state->updateState(facebook::react::ModalHostViewState(
          facebook::react::Size{.width = width, .height = height}));
    }
  }
  return MountError::None;
}

void ShadowViewMutationAdapter::surfaceResized() {
  const auto modals = modalViews_;
  for (const auto& view : modals) {
    applyModal(view);
  }
}

void ShadowViewMutationAdapter::connectSwitchEvents(const ShadowView& view) {
  if (switchBridges_.contains(view.tag)) {
    return;
  }

  const auto emitter =
      std::dynamic_pointer_cast<const facebook::react::SwitchEventEmitter>(
          view.eventEmitter);
  if (emitter == nullptr) {
    return;
  }

  QQuickItem* item = layer_.itemForTag(view.tag);
  if (item == nullptr) {
    return;
  }

  const auto* props =
      dynamic_cast<const facebook::react::SwitchProps*>(view.props.get());
  const bool initialValue = props != nullptr ? props->value : false;

  const auto emitChange = [emitter, viewTag = view.tag](bool value) {
    facebook::react::SwitchEventEmitter::OnChange event;
    event.value = value;
    event.target = static_cast<int>(viewTag);
    emitter->onChange(event);
  };

  auto* bridge =
      new QtSwitchBridge(emitChange, item, initialValue, item);
  QObject::connect(
      item, SIGNAL(valueChanged()), bridge, SLOT(notify()));

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

      const MountError error = layer_.create(view.tag, *qmlType);
      if (error != MountError::None) {
        detail = layer_.lastError();
        return error;
      }
      if (const auto modalError = applyModal(view); modalError != MountError::None) {
        detail = layer_.lastError();
        return modalError;
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
      if (const auto& observer = viewObserver()) observer(view, false);
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
#if RNQNX_ENABLE_QT_SVG
      applySvgDocument(view);
#endif
      applyExtraComponentProps(layer_.itemForTag(view.tag), view);
      const auto styleError = layer_.updateStyle(view.tag, toViewStyle(view));
      if (styleError != MountError::None) return styleError;
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
      return applyAccessibilityActions(view, detail);
#else
      return MountError::None;
#endif
    }

    case ShadowViewMutation::Type::Delete:
#if RNQNX_ENABLE_QT_SVG
      svgBridges_.remove(mutation.oldChildShadowView.tag);
#endif
#if RNQNX_ENABLE_ACCESSIBILITY_BRIDGE
      if (auto previous = accessibilityActivations_.take(mutation.oldChildShadowView.tag)) {
        previous->invalidate(); delete previous.data();
      }
#endif
      if (const auto& observer = viewObserver()) observer(mutation.oldChildShadowView, true);
      if (touchDispatcher_ != nullptr) {
        touchDispatcher_->forgetTag(mutation.oldChildShadowView.tag);
      }
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
      textInputConnected_.remove(mutation.oldChildShadowView.tag);
#endif
      switchBridges_.remove(mutation.oldChildShadowView.tag);
      modalViews_.remove(mutation.oldChildShadowView.tag);
      imageBridges_.remove(mutation.oldChildShadowView.tag);
      return layer_.destroy(mutation.oldChildShadowView.tag);

    case ShadowViewMutation::Type::Insert: {
      const MountError error = layer_.insert(
          mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);
      if (error != MountError::None) {
        return error;
      }
#if RNQNX_ENABLE_QT_RNSVG
      // stackBefore() reorders siblings without any item-change notification,
      // and paint order is the <Svg> children's order.
      if (auto* svgChild = qobject_cast<views::RnsvgElementItem*>(layer_.itemForTag(mutation.newChildShadowView.tag))) {
        if (auto* svgRoot = svgChild->root()) svgRoot->invalidate();
      }
#endif
      // Qt's Flickable does not derive its content size from its children, so
      // it is recomputed whenever they change.
      return layer_.updateScrollContentSize(mutation.parentTag);
    }

    case ShadowViewMutation::Type::Remove: {
      const MountError error = layer_.remove(
          mutation.parentTag, mutation.oldChildShadowView.tag, mutation.index);
      if (error != MountError::None) {
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
      if (const auto& observer = viewObserver()) observer(view, false);
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
#if RNQNX_ENABLE_QT_SVG
      applySvgDocument(view);
#endif
      applyExtraComponentProps(layer_.itemForTag(view.tag), view);
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
