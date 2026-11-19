#pragma once

#include <QString>

#include <string>
#include <vector>

class QQuickItem;

namespace facebook::react {
class ComponentDescriptorProviderRegistry;
struct ShadowView;
}  // namespace facebook::react

namespace rnqnx::fabric {

/// The <filter> element and the Fe* primitives of react-native-svg: the eight
/// components the package mounts (RNSVGFilter, RNSVGFeBlend, RNSVGFeColorMatrix,
/// RNSVGFeComposite, RNSVGFeFlood, RNSVGFeGaussianBlur, RNSVGFeMerge,
/// RNSVGFeOffset) and the ten that packages/react-native-qnx/svg adds because
/// the package declares them in JavaScript and never renders them
/// (RNSVGFeMorphology, RNSVGFeTurbulence, RNSVGFeDisplacementMap,
/// RNSVGFeComponentTransfer, RNSVGFeDropShadow, RNSVGFeImage,
/// RNSVGFeConvolveMatrix, RNSVGFeTile, RNSVGFeDiffuseLighting,
/// RNSVGFeSpecularLighting).
bool isRnsvgFilterComponent(const std::string& componentName);
std::vector<std::string> rnsvgFilterComponentNames();

/// Registers the descriptors. Shadow nodes come from the pinned package's own
/// C++ for the eight it mounts and from the same template for the other ten.
void addRnsvgFilterDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);

/// Converts the view's generated props into the element's retained props
/// (Kind::Filter, or Kind::FilterPrimitive with `ElementProps::primitive`) and
/// applies them without remounting. False when `item` or the props are not
/// those of a filter component.
bool applyRnsvgFilterProps(QQuickItem* item, const facebook::react::ShadowView& view);

/// The element item for a filter component name, or nullptr when the name is
/// not one of them.
QQuickItem* createRnsvgFilterItem(const QString& componentName, QQuickItem* parent);

/// Names the shared bridge (QtRnsvgBridge.cpp) was written against.
inline bool isRnsvgFilterPrimitive(const std::string& componentName) {
  return isRnsvgFilterComponent(componentName);
}
inline std::vector<std::string> rnsvgFilterPrimitiveNames() { return rnsvgFilterComponentNames(); }

}  // namespace rnqnx::fabric
