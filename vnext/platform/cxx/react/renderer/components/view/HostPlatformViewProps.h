#pragma once

#include <react/renderer/components/view/BaseViewProps.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

// RN's platform extension point, selected for the entire native graph when
// desktop input is enabled. Installed React Native sources remain untouched.
class HostPlatformViewProps : public BaseViewProps {
 public:
  HostPlatformViewProps() = default;
  HostPlatformViewProps(
      const PropsParserContext& context,
      const HostPlatformViewProps& source,
      const RawProps& raw,
      const std::function<bool(const std::string&)>& filter = nullptr)
      : BaseViewProps(context, source, raw, filter),
        focusable(convertRawProp(context, raw, "focusable", source.focusable, false)),
        onKeyDown(convertRawProp(context, raw, "onKeyDown", source.onKeyDown, false)),
        onKeyDownCapture(convertRawProp(context, raw, "onKeyDownCapture", source.onKeyDownCapture, false)),
        onKeyUp(convertRawProp(context, raw, "onKeyUp", source.onKeyUp, false)),
        onKeyUpCapture(convertRawProp(context, raw, "onKeyUpCapture", source.onKeyUpCapture, false)) {}

  void setProp(const PropsParserContext& context, RawPropsPropNameHash hash,
               const char* name, const RawValue& value) {
    BaseViewProps::setProp(context, hash, name, value);
    switch (hash) {
      case CONSTEXPR_RAW_PROPS_KEY_HASH("focusable"):
        fromRawValue(context, value, focusable, false); break;
      case CONSTEXPR_RAW_PROPS_KEY_HASH("onKeyDown"):
        fromRawValue(context, value, onKeyDown, false); break;
      case CONSTEXPR_RAW_PROPS_KEY_HASH("onKeyDownCapture"):
        fromRawValue(context, value, onKeyDownCapture, false); break;
      case CONSTEXPR_RAW_PROPS_KEY_HASH("onKeyUp"):
        fromRawValue(context, value, onKeyUp, false); break;
      case CONSTEXPR_RAW_PROPS_KEY_HASH("onKeyUpCapture"):
        fromRawValue(context, value, onKeyUpCapture, false); break;
    }
  }

  bool focusable{false};
  bool onKeyDown{false};
  bool onKeyDownCapture{false};
  bool onKeyUp{false};
  bool onKeyUpCapture{false};
};

}  // namespace facebook::react
