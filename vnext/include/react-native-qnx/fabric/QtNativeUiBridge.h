#pragma once

#include <string>
#include <functional>
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
/// Owner-thread non-pointer click activation for accessible mounted React views.
void setNativeUiPressCallback(QQuickItem* item, std::function<void()> callback);

/// ViewManagerAdapter_ExpoImage, the view expo-image's requireNativeView
/// resolves (RNQNX_ENABLE_QT_EXPO_IMAGE).
std::vector<std::string> expoImageComponentNames();
void addExpoImageComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry);
bool applyExpoImageProps(QQuickItem* item, const facebook::react::ShadowView& view);
/// `startAnimating`, `stopAnimating`, `lockResource`, `unlockResource` and
/// `reload`, the commands of ExpoImageNativeComponent.ts.
bool dispatchExpoImageCommand(QQuickItem* item, const std::string& command);

}  // namespace rnqnx::fabric
