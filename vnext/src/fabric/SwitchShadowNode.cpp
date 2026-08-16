/*
 * QNX implementation for React Native's iOS switch shadow node.
 *
 * The iOS switch component is a platform-forked descriptor under
 * react/renderer/components/switch/platform/ios. The .mm implementations are
 * Apple-only, so this file provides the component name and a default intrinsic
 * size that keeps the shadow node measurable on a non-Apple host.
 */

#include <react/renderer/components/switch/iosswitch/react/renderer/components/switch/AppleSwitchShadowNode.h>
#include <react/renderer/core/LayoutConstraints.h>

#include <algorithm>

namespace facebook::react {

extern const char AppleSwitchComponentName[] = "Switch";

Size SwitchShadowNode::measureContent(
    const LayoutContext& layoutContext,
    const LayoutConstraints& layoutConstraints) const {
  // iOS UISwitch's default intrinsic size is 51x31 points. This is a stand-in
  // until QNX has a reason to measure the QML item at layout time.
  (void)layoutContext;
  const float width =
      std::clamp(51.0F, layoutConstraints.minimumSize.width, layoutConstraints.maximumSize.width);
  const float height =
      std::clamp(31.0F, layoutConstraints.minimumSize.height, layoutConstraints.maximumSize.height);
  return Size{width, height};
}

}  // namespace facebook::react
