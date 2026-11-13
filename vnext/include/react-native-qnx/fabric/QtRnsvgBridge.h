#pragma once

#include <memory>
#include <string>
#include <vector>

class QQuickItem;

namespace facebook::react {
class ComponentDescriptorProviderRegistry;
struct ShadowView;
}  // namespace facebook::react

namespace rnqnx::fabric {

/// The react-native-svg Fabric components this host mounts. Pattern, Symbol,
/// Marker, TextPath, Filter, Fe* and ForeignObject are deliberately absent:
/// rendering one fails with React Native's unknown-component error instead of
/// silently drawing nothing.
std::vector<std::string> rnsvgComponentNames();

/// Registers the descriptors. Shadow nodes and layout come from the pinned
/// react-native-svg package's own C++, so Yoga treats elements as it does there.
void addRnsvgComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);

bool isRnsvgComponent(const std::string& componentName);

/// Converts the view's generated props into the retained element/root props and
/// applies them to `item` without remounting. Returns false for a view that is
/// not a react-native-svg component or whose props do not match its item.
bool applyRnsvgProps(QQuickItem* item, const facebook::react::ShadowView& view);

}  // namespace rnqnx::fabric
