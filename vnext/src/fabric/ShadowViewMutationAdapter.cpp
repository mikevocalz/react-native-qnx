#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"

#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/components/view/ViewProps.h>
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

void ShadowViewMutationAdapter::mapComponent(
    const QString& componentName,
    const QString& qmlTypeName) {
  componentTypes_.insert(componentName, qmlTypeName);
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
      return layer_.updateStyle(view.tag, toViewStyle(view));
    }

    case ShadowViewMutation::Type::Delete:
      return layer_.destroy(mutation.oldChildShadowView.tag);

    case ShadowViewMutation::Type::Insert:
      return layer_.insert(
          mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);

    case ShadowViewMutation::Type::Remove:
      return layer_.remove(
          mutation.parentTag, mutation.oldChildShadowView.tag, mutation.index);

    case ShadowViewMutation::Type::Update: {
      const ShadowView& view = mutation.newChildShadowView;
      const MountError layoutError =
          layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
      if (layoutError != MountError::None) {
        return layoutError;
      }
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
