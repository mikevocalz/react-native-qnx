#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"

#include <QColor>
#include <QStringList>

#include <react/renderer/core/LayoutMetrics.h>
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

void ShadowViewMutationAdapter::applyText(const ShadowView& view) {
  const TextStyle style = paragraphTextStyle(view);
  if (!style.text.isEmpty()) {
    layer_.updateText(view.tag, style);
  }
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
      applyText(view);
      return layer_.updateStyle(view.tag, toViewStyle(view));
    }

    case ShadowViewMutation::Type::Delete:
      if (touchDispatcher_ != nullptr) {
        touchDispatcher_->forgetTag(mutation.oldChildShadowView.tag);
      }
      return layer_.destroy(mutation.oldChildShadowView.tag);

    case ShadowViewMutation::Type::Insert:
      return layer_.insert(
          mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);

    case ShadowViewMutation::Type::Remove:
      return layer_.remove(
          mutation.parentTag, mutation.oldChildShadowView.tag, mutation.index);

    case ShadowViewMutation::Type::Update: {
      const ShadowView& view = mutation.newChildShadowView;
      registerEventEmitter(view);
      const MountError layoutError =
          layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
      if (layoutError != MountError::None) {
        return layoutError;
      }
      applyText(view);
      return layer_.updateStyle(view.tag, toViewStyle(view));
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
