#pragma once

#include <string>
#include <vector>

class QQuickItem;

namespace facebook::react {
class ComponentDescriptorProviderRegistry;
struct ShadowView;
}  // namespace facebook::react

namespace rnqnx::fabric {

// Fabric half of the Qt native UI components. Each trio below is one
// ExtraComponentSet (QtExtraComponents.cpp). The definitions exist only in a
// build that enables the matching CMake option.

/// RNQnxDrawer and RNQnxTabBar (RNQNX_ENABLE_QT_NATIVE_UI).
std::vector<std::string> nativeUiComponentNames();
void addNativeUiComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);
bool applyNativeUiProps(QQuickItem* item, const facebook::react::ShadowView& view);

/// ViewManagerAdapter_ExpoImage, the view expo-image's requireNativeView
/// resolves (RNQNX_ENABLE_QT_EXPO_IMAGE).
std::vector<std::string> expoImageComponentNames();
void addExpoImageComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);
bool applyExpoImageProps(QQuickItem* item, const facebook::react::ShadowView& view);

}  // namespace rnqnx::fabric
