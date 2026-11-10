#pragma once
#include <react/renderer/components/view/HostPlatformViewProps.h>

namespace facebook::react::HostPlatformViewTraitsInitializer {
  inline bool formsView(const HostPlatformViewProps& props) {
    return props.focusable || props.onKeyDown || props.onKeyDownCapture ||
        props.onKeyUp || props.onKeyUpCapture;
  }
  inline bool formsStackingContext(const HostPlatformViewProps& props) {
    return formsView(props);
  }
  inline bool isKeyboardFocusable(const HostPlatformViewProps& props) {
    return props.focusable && !(props.accessibilityState && props.accessibilityState->disabled);
  }
}  // namespace facebook::react::HostPlatformViewTraitsInitializer
