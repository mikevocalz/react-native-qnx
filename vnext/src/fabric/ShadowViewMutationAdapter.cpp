#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"

#include "react-native-qnx/fabric/QtScrollBridge.h"

#include <react/renderer/components/iostextinput/TextInputProps.h>
#include <react/renderer/components/textinput/TextInputEventEmitter.h>

#include <QColor>
#include <QMetaMethod>
#include <QQuickItem>
#include <QStringList>

#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/components/image/ImageProps.h>
#include <react/renderer/components/scrollview/ScrollViewEventEmitter.h>
#include <react/renderer/components/text/ParagraphState.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/ConcreteState.h>
#include <react/renderer/graphics/Color.h>
#include <react/renderer/mounting/ShadowView.h>

namespace rnqnx::fabric {
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
  return parts.join(QLatin1Char(';'));
}

/// The first image source's URI, or empty when the view is not an Image.
///
/// React Native's ImageProps carries a list so a platform can pick by scale or
/// size (`components/image/ImageProps.h:28`). This takes the first: Qt Quick's
/// Image loads one URL and does its own scaling, and choosing among candidates
/// needs the display metrics work that is not done yet.
QString imageSourceUri(const ShadowView& view) {
  const auto* props =
      dynamic_cast<const facebook::react::ImageProps*>(view.props.get());
  if (props == nullptr || props->sources.empty()) {
    return {};
  }
  return QString::fromStdString(props->sources.front().uri);
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
    }
  }

  // More than one styled fragment means Qt Quick's single font and colour
  // cannot express the paragraph, so the rich-text form is used instead.
  style.mixedStyles = fragmentCount > 1;
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
  // collapses. Taking the top-left is faithful when all four agree and lossy
  // otherwise; the loss is recorded in docs/component-support.md.
  style.borderRadius = border.borderRadii.topLeft.horizontal;
  style.borderWidth = static_cast<float>(border.borderWidths.left);

  if (facebook::react::isColorMeaningful(border.borderColors.left)) {
    style.hasBorderColor = true;
    style.borderColor = packColor(border.borderColors.left);
  }

  return style;
}

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
  const QString uri = imageSourceUri(view);
  if (!uri.isEmpty()) {
    layer_.updateImageSource(view.tag, uri);
  }
}

void ShadowViewMutationAdapter::applyText(const ShadowView& view) {
  const TextStyle style = paragraphTextStyle(view);
  if (!style.text.isEmpty()) {
    layer_.updateText(view.tag, style);
  }
}

void ShadowViewMutationAdapter::applyTextInput(const ShadowView& view) {
  const auto* props =
      dynamic_cast<const facebook::react::TextInputProps*>(view.props.get());
  if (props == nullptr) {
    return;
  }

  TextInputStyle style;
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

  layer_.updateTextInput(view.tag, style);
}

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

  QQuickItem* item = layer_.itemForTag(view.tag);
  // A plain Item has no `text` property, so this also rejects a TextInput
  // component mapped onto a QML type that cannot edit.
  if (item == nullptr || !item->property("text").isValid()) {
    return;
  }

  // eventCount is React Native's ordering token: JavaScript compares it against
  // the count it last sent so it can drop an update that crossed a keystroke in
  // flight. It has to increase per emission, so it is owned here.
  auto eventCount = std::make_shared<int>(0);
  const auto emitChange = [emitter, item, eventCount]() {
    facebook::react::TextInputEventEmitter::Metrics metrics{};
    metrics.text = item->property("text").toString().toStdString();
    metrics.eventCount = ++(*eventCount);
    metrics.containerSize = {
        .width = static_cast<facebook::react::Float>(item->width()),
        .height = static_cast<facebook::react::Float>(item->height())};
    metrics.layoutMeasurement = metrics.containerSize;
    metrics.zoomScale = 1.0F;
    emitter->onChange(metrics);
  };

  // The bridge is parented to the item, so it dies with it.
  auto* bridge = new QtScrollBridge(emitChange, item);
  QObject::connect(item, SIGNAL(textChanged()), bridge, SLOT(notify()));

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

  // Qt emits contentXChanged/contentYChanged as the Flickable moves. The item
  // is the connection context, so the handler dies with it.
  const auto emitScroll = [emitter, item]() {
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
    event.zoomScale = 1.0F;
    emitter->onScroll(event);
  };

  // The bridge is parented to the item, so it dies with it.
  auto* bridge = new QtScrollBridge(emitScroll, item);
  QObject::connect(item, SIGNAL(contentXChanged()), bridge, SLOT(notify()));
  QObject::connect(item, SIGNAL(contentYChanged()), bridge, SLOT(notify()));

  scrollConnected_.insert(view.tag);
}

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

      // Create carries the first layout and props, so applying both here avoids
      // a frame at the origin with zero size and no colour before the first
      // Update arrives.
      const MountError layoutError =
          layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
      if (layoutError != MountError::None) {
        return layoutError;
      }
      registerEventEmitter(view);
      connectScrollEvents(view);
      connectTextInputEvents(view);
      applyText(view);
      applyImage(view);
      applyTextInput(view);
      return layer_.updateStyle(view.tag, toViewStyle(view));
    }

    case ShadowViewMutation::Type::Delete:
      if (touchDispatcher_ != nullptr) {
        touchDispatcher_->forgetTag(mutation.oldChildShadowView.tag);
      }
      return layer_.destroy(mutation.oldChildShadowView.tag);

    case ShadowViewMutation::Type::Insert: {
      const MountError error = layer_.insert(
          mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);
      if (error != MountError::None) {
        return error;
      }
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
      registerEventEmitter(view);
      const MountError layoutError =
          layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
      if (layoutError != MountError::None) {
        return layoutError;
      }
      connectTextInputEvents(view);
      applyText(view);
      applyImage(view);
      applyTextInput(view);
      const MountError styleError = layer_.updateStyle(view.tag, toViewStyle(view));
      if (styleError != MountError::None) {
        return styleError;
      }
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

}  // namespace rnqnx::fabric
