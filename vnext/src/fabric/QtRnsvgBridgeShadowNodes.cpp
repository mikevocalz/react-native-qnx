// Shadow nodes and descriptors the host supplies for react-native-svg. The
// package's own RNSVGShadowNodes.cpp includes a generated header this host
// does not produce, so it is not compiled and this file defines what it would.

#include "QtRnsvgBridgeInternal.h"

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/components/rnsvg/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "RNSVGComponentDescriptors.h"
#include "RNSVGConcreteShadowNode.h"
#include "RNSVGShadowNodes.h"

namespace facebook::react {
// Names for the shadow nodes this host mounts: the Fabric component names the
// package registers. The Filter and Fe* names are defined by the filter bridge
// (QtRnsvgFilterBridge), which registers those descriptors.
extern const char RNSVGCircleComponentName[] = "RNSVGCircle";
extern const char RNSVGClipPathComponentName[] = "RNSVGClipPath";
extern const char RNSVGDefsComponentName[] = "RNSVGDefs";
extern const char RNSVGEllipseComponentName[] = "RNSVGEllipse";
extern const char RNSVGForeignObjectComponentName[] = "RNSVGForeignObject";
extern const char RNSVGGroupComponentName[] = "RNSVGGroup";
extern const char RNSVGLinearGradientComponentName[] = "RNSVGLinearGradient";
extern const char RNSVGLineComponentName[] = "RNSVGLine";
extern const char RNSVGMarkerComponentName[] = "RNSVGMarker";
extern const char RNSVGMaskComponentName[] = "RNSVGMask";
extern const char RNSVGPathComponentName[] = "RNSVGPath";
extern const char RNSVGPatternComponentName[] = "RNSVGPattern";
extern const char RNSVGRadialGradientComponentName[] = "RNSVGRadialGradient";
extern const char RNSVGRectComponentName[] = "RNSVGRect";
extern const char RNSVGSymbolComponentName[] = "RNSVGSymbol";
extern const char RNSVGTextComponentName[] = "RNSVGText";
extern const char RNSVGTextPathComponentName[] = "RNSVGTextPath";
extern const char RNSVGTSpanComponentName[] = "RNSVGTSpan";
extern const char RNSVGUseComponentName[] = "RNSVGUse";
// react-native-svg's own Image shadow node needs an ImageManager and a state
// round trip this renderer does not use (it decodes the source itself), so the
// plain element shadow node is used for RNSVGImage.
constexpr char RNSVGImageComponentName[] = "RNSVGImage";
using RNSVGImageShadowNode = RNSVGConcreteShadowNode<RNSVGImageComponentName, RNSVGImageProps>;
// <Svg> takes its size from ordinary Yoga style, unlike its elements. It must
// still be mounted (not flattened) so its Qt item exists to hold the tree.
constexpr char RNSVGSvgViewComponentName[] = "RNSVGSvgView";
class RNSVGSvgViewShadowNode final
    : public ConcreteViewShadowNode<RNSVGSvgViewComponentName, RNSVGSvgViewProps, ViewEventEmitter> {
 public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;
  static ShadowNodeTraits BaseTraits() {
    auto traits = ConcreteViewShadowNode::BaseTraits();
    traits.set(ShadowNodeTraits::Trait::FormsView);
    traits.set(ShadowNodeTraits::Trait::FormsStackingContext);
    return traits;
  }
};
}  // namespace facebook::react

namespace rnqnx::fabric::rnsvg_bridge {

// RNSVGForeignObjectShadowNode is the package's YogaLayoutableShadowNode
// flavour (no absolute-fill override), so the React Native views under a
// <ForeignObject> are laid out by Yoga inside it. Every other element here is
// the absolute-fill flavour.
void addCoreDescriptors(rn::ComponentDescriptorProviderRegistry& registry) {
  using namespace rn;
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGSvgViewShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<RNSVGGroupComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGDefsComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGPathComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGCircleComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGEllipseComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGRectComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGLineComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGLinearGradientComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGRadialGradientComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGClipPathComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGMaskComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGPatternComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGSymbolComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGMarkerComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGTextComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGTSpanComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGTextPathComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGUseComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGForeignObjectComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGImageShadowNode>>());
}

}  // namespace rnqnx::fabric::rnsvg_bridge
