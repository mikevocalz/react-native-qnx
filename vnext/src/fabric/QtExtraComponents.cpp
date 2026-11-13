#include "react-native-qnx/fabric/QtExtraComponents.h"

#include <react/renderer/mounting/ShadowView.h>

#include <unordered_map>
#include <utility>

#if RNQNX_ENABLE_QT_RNSVG
#include "react-native-qnx/fabric/QtRnsvgBridge.h"
#endif
#if RNQNX_ENABLE_QT_NATIVE_UI || RNQNX_ENABLE_QT_EXPO_IMAGE
#include "react-native-qnx/fabric/QtNativeUiBridge.h"
#endif

namespace rnqnx::fabric {
namespace {

// The one list. A feature appears here once, behind its own switch.
std::vector<ExtraComponentSet> buildSets() {
  std::vector<ExtraComponentSet> sets;
#if RNQNX_ENABLE_QT_RNSVG
  sets.push_back(ExtraComponentSet{
      "react-native-svg",
      &rnsvgComponentNames,
      &addRnsvgComponentDescriptors,
      [](QQuickItem* item, const facebook::react::ShadowView& view) {
        return applyRnsvgProps(item, view);
      }});
#endif
#if RNQNX_ENABLE_QT_NATIVE_UI
  sets.push_back(ExtraComponentSet{
      "qt-native-ui",
      &nativeUiComponentNames,
      &addNativeUiComponentDescriptors,
      &applyNativeUiProps});
#endif
#if RNQNX_ENABLE_QT_EXPO_IMAGE
  sets.push_back(ExtraComponentSet{
      "expo-image",
      &expoImageComponentNames,
      &addExpoImageComponentDescriptors,
      &applyExpoImageProps});
#endif
  return sets;
}

}  // namespace

const std::vector<ExtraComponentSet>& extraComponentSets() {
  static const std::vector<ExtraComponentSet> sets = buildSets();
  return sets;
}

namespace {

// Name -> owning set, built once: applyExtraComponentProps runs for every view
// mutation, so it must not rebuild name lists.
const std::unordered_map<std::string, const ExtraComponentSet*>& owners() {
  static const auto map = [] {
    std::unordered_map<std::string, const ExtraComponentSet*> built;
    for (const auto& set : extraComponentSets()) {
      for (auto& name : set.names()) {
        built.emplace(std::move(name), &set);
      }
    }
    return built;
  }();
  return map;
}

}  // namespace

std::vector<std::string> extraComponentNames() {
  std::vector<std::string> all;
  for (const auto& set : extraComponentSets()) {
    for (auto& name : set.names()) {
      all.push_back(std::move(name));
    }
  }
  return all;
}

void addExtraComponentDescriptors(facebook::react::ComponentDescriptorProviderRegistry& registry) {
  for (const auto& set : extraComponentSets()) {
    set.addDescriptors(registry);
  }
}

bool isExtraComponent(const std::string& componentName) {
  return owners().count(componentName) != 0;
}

bool applyExtraComponentProps(QQuickItem* item, const facebook::react::ShadowView& view) {
  if (item == nullptr || view.componentName == nullptr) {
    return false;
  }
  const auto owner = owners().find(view.componentName);
  return owner != owners().end() && owner->second->applyProps(item, view);
}

}  // namespace rnqnx::fabric
