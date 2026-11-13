#pragma once

#include <string>
#include <vector>

class QQuickItem;

namespace facebook::react {
class ComponentDescriptorProviderRegistry;
struct ShadowView;
}  // namespace facebook::react

namespace rnqnx::fabric {

/// One feature's extra Fabric components. Adding a component means adding one
/// entry to the table in QtExtraComponents.cpp (and its item to
/// views/QtExtraItems.cpp); QtReactHost, the mutation adapter and the item
/// factory read these lists and need no per-feature code.
struct ExtraComponentSet {
  /// The CMake feature, for diagnostics.
  const char* feature;
  /// Fabric component names this set mounts. They are what
  /// UIManager.hasViewManagerConfig reports.
  std::vector<std::string> (*names)();
  /// Adds this set's component descriptors to the provider registry.
  void (*addDescriptors)(facebook::react::ComponentDescriptorProviderRegistry& registry);
  /// Applies the view's props to its mounted item and wires its events. Called
  /// on Create and on every Update of a component named in `names`. Returns
  /// false when the item or props do not match the component.
  bool (*applyProps)(QQuickItem* item, const facebook::react::ShadowView& view);
};

/// The compiled-in sets, in registration order.
const std::vector<ExtraComponentSet>& extraComponentSets();

/// Every extra component name, for the registry list and the JS predicate.
std::vector<std::string> extraComponentNames();

/// Adds every extra set's descriptors.
void addExtraComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);

/// True when \a componentName belongs to an extra set.
bool isExtraComponent(const std::string& componentName);

/// Routes \a view to the set that owns its component. Returns false for a view
/// no set owns (the caller then does nothing) or when the set rejects it.
bool applyExtraComponentProps(QQuickItem* item, const facebook::react::ShadowView& view);

}  // namespace rnqnx::fabric
