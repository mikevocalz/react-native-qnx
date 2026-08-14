#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"

#include <react/renderer/core/LayoutMetrics.h>
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

      // Create carries the first layout, so applying it here avoids a frame at
      // the origin with zero size before the first Update arrives.
      return layer_.updateLayout(view.tag, toLayoutFrame(view.layoutMetrics));
    }

    case ShadowViewMutation::Type::Delete:
      return layer_.destroy(mutation.oldChildShadowView.tag);

    case ShadowViewMutation::Type::Insert:
      return layer_.insert(
          mutation.parentTag, mutation.newChildShadowView.tag, mutation.index);

    case ShadowViewMutation::Type::Remove:
      return layer_.remove(
          mutation.parentTag, mutation.oldChildShadowView.tag, mutation.index);

    case ShadowViewMutation::Type::Update:
      // Props and state are not consumed yet; only layout is applied. Anything
      // beyond geometry is an honest gap recorded in component-support docs
      // rather than a silent partial update.
      return layer_.updateLayout(
          mutation.newChildShadowView.tag,
          toLayoutFrame(mutation.newChildShadowView.layoutMetrics));
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
