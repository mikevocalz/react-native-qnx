#include "react-native-qnx/fabric/QtRnsvgFilterBridge.h"

#include "QtRnsvgFilterConvert.h"

#include <RnsvgItems.h>

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/components/rnsvg/Props.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/mounting/ShadowView.h>

#include "RNSVGComponentDescriptors.h"
#include "RNSVGConcreteShadowNode.h"
#include "RNSVGShadowNodes.h"

#include <QQuickItem>

#include <algorithm>
#include <unordered_map>

namespace facebook::react {
// The package's RNSVGShadowNodes.cpp is not compiled (it includes a generated
// header this host does not produce), so the names its eight filter shadow
// nodes refer to are defined here.
extern const char RNSVGFeBlendComponentName[] = "RNSVGFeBlend";
extern const char RNSVGFeColorMatrixComponentName[] = "RNSVGFeColorMatrix";
extern const char RNSVGFeCompositeComponentName[] = "RNSVGFeComposite";
extern const char RNSVGFeFloodComponentName[] = "RNSVGFeFlood";
extern const char RNSVGFeGaussianBlurComponentName[] = "RNSVGFeGaussianBlur";
extern const char RNSVGFeMergeComponentName[] = "RNSVGFeMerge";
extern const char RNSVGFeOffsetComponentName[] = "RNSVGFeOffset";
extern const char RNSVGFilterComponentName[] = "RNSVGFilter";

// The ten primitives the package declares without a shadow node of its own use
// the same node type as the eight it does.
#define RNQNX_RNSVG_FILTER_SHADOW_NODE(Name)                                                     \
  constexpr char RNSVG##Name##ComponentName[] = "RNSVG" #Name;                                   \
  using RNSVG##Name##ShadowNode = RNSVGConcreteShadowNode<RNSVG##Name##ComponentName, RNSVG##Name##Props>
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeMorphology);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeTurbulence);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeDisplacementMap);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeComponentTransfer);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeDropShadow);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeImage);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeConvolveMatrix);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeTile);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeDiffuseLighting);
RNQNX_RNSVG_FILTER_SHADOW_NODE(FeSpecularLighting);
#undef RNQNX_RNSVG_FILTER_SHADOW_NODE
}  // namespace facebook::react

namespace rnqnx::fabric {

namespace rn = facebook::react;
using namespace rnsvg_bridge;

namespace {

const std::unordered_map<std::string, Entry>& table() {
  static const std::unordered_map<std::string, Entry> t = [] {
    std::vector<Entry> rows;
    appendFilterEntries(rows);
    appendFilterExtraEntries(rows);
    std::unordered_map<std::string, Entry> built;
    for (const Entry& row : rows) built.emplace(row.name, row);
    return built;
  }();
  return t;
}

}  // namespace

bool isRnsvgFilterComponent(const std::string& componentName) {
  return table().count(componentName) != 0;
}

std::vector<std::string> rnsvgFilterComponentNames() {
  std::vector<std::string> names;
  names.reserve(table().size());
  for (const auto& [name, entry] : table()) names.push_back(name);
  std::sort(names.begin(), names.end());
  return names;
}

void addRnsvgFilterDescriptors(rn::ComponentDescriptorProviderRegistry& registry) {
  using namespace rn;
  registry.add(concreteComponentDescriptorProvider<RNSVGFilterComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeBlendComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeColorMatrixComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeCompositeComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeFloodComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeGaussianBlurComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeMergeComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGFeOffsetComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeMorphologyShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeTurbulenceShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeDisplacementMapShadowNode>>());
  registry.add(
      concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeComponentTransferShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeDropShadowShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeImageShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeConvolveMatrixShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeTileShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeDiffuseLightingShadowNode>>());
  registry.add(
      concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGFeSpecularLightingShadowNode>>());
}

bool applyRnsvgFilterProps(QQuickItem* item, const rn::ShadowView& view) {
  if (view.componentName == nullptr || !view.props) return false;
  const auto it = table().find(view.componentName);
  auto* element = qobject_cast<views::RnsvgElementItem*>(item);
  if (it == table().end() || !element) return false;
  ElementProps next;
  next.kind = it->second.kind;
  if (!it->second.convert(*view.props, next, element->props())) return false;
  element->setProps(std::move(next));
  return true;
}

QQuickItem* createRnsvgFilterItem(const QString& componentName, QQuickItem* parent) {
  const auto it = table().find(componentName.toStdString());
  if (it == table().end()) return nullptr;
  return new views::RnsvgElementItem(it->second.kind, parent);
}

}  // namespace rnqnx::fabric
