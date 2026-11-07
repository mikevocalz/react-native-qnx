include_guard(GLOBAL)

# GCC rejects these unqualified names when the class later introduces members
# with the same names. Qualification selects the same namespace-level types
# without relaxing diagnostics or changing React Native's ABI.
set(_rnqnx_concrete_shadow_node
  "${REACT_COMMON_DIR}/react/renderer/core/ConcreteShadowNode.h")
rnqnx_patch_qnx_source("${_rnqnx_concrete_shadow_node}"
  "std::is_base_of<Props, PropsT>"
  "std::is_base_of<::facebook::react::Props, PropsT>")
rnqnx_patch_qnx_source("${_rnqnx_concrete_shadow_node}"
  "using ConcreteState = ConcreteState<StateDataT>;"
  "using ConcreteState = ::facebook::react::ConcreteState<StateDataT>;")
rnqnx_patch_qnx_source("${_rnqnx_concrete_shadow_node}"
  "Props(const PropsParserContext &context, const RawProps &rawProps, const Props::Shared &baseProps = nullptr)"
  "Props(const PropsParserContext &context, const RawProps &rawProps, const ::facebook::react::Props::Shared &baseProps = nullptr)")
rnqnx_patch_qnx_source("${_rnqnx_concrete_shadow_node}"
  "static UnsharedConcreteProps Props(const Props::Shared &baseProps)"
  "static UnsharedConcreteProps Props(const ::facebook::react::Props::Shared &baseProps)")
rnqnx_patch_qnx_source("${_rnqnx_concrete_shadow_node}"
  "      const RawProps &rawProps,\n      const Props::Shared &baseProps = nullptr)"
  "      const RawProps &rawProps,\n      const ::facebook::react::Props::Shared &baseProps = nullptr)")
rnqnx_patch_qnx_source("${_rnqnx_concrete_shadow_node}"
  "      const Props::Shared & /*props*/,"
  "      const ::facebook::react::Props::Shared & /*props*/,")
unset(_rnqnx_concrete_shadow_node)

# This warning name belongs to Clang; GCC otherwise diagnoses the diagnostic
# directive itself under RN's -Werror setting.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/featureflags/ReactNativeFeatureFlags.cpp"
  "#pragma GCC diagnostic ignored \"-Wglobal-constructors\""
  "#if defined(__clang__)\n#pragma GCC diagnostic ignored \"-Wglobal-constructors\"\n#endif")

# Keep the bounds check in the existing signed length domain.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/mapbuffer/MapBuffer.cpp"
  "curLen + sizeof(int32_t) > mapBufferListLength"
  "mapBufferListLength - curLen < static_cast<int32_t>(sizeof(int32_t))")

# GCC requires an explicit invalid-enum path after the exhaustive switch.
# Fail closed rather than manufacturing props for a corrupt mode.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/core/RawProps.cpp"
  "#include \"RawProps.h\""
  "#include \"RawProps.h\"\n#include <cstdlib>")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/core/RawProps.cpp"
  "    case Mode::Dynamic:\n      return dynamic_;\n  }\n}"
  "    case Mode::Dynamic:\n      return dynamic_;\n  }\n  std::abort();\n}")

# Header-defined template argument needs one external inline identity.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/components/view/LayoutConformanceShadowNode.h"
  "constexpr const char LayoutConformanceShadowNodeComponentName[]"
  "inline constexpr const char LayoutConformanceShadowNodeComponentName[]")

# Preserve valid enum conversions; reject invalid enum values explicitly.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/components/view/conversions.h"
  "#include <cmath>"
  "#include <cmath>\n#include <cstdlib>")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/components/view/conversions.h"
  "      return PositionType::Absolute;\n  }\n}"
  "      return PositionType::Absolute;\n  }\n  std::abort();\n}")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/components/view/conversions.h"
  "      return DisplayType::Grid;\n  }\n}"
  "      return DisplayType::Grid;\n  }\n  std::abort();\n}")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/components/view/conversions.h"
  "      return YGDirectionRTL;\n  }\n}"
  "      return YGDirectionRTL;\n  }\n  std::abort();\n}")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/mounting/ShadowTree.cpp"
  "#include \"ShadowTree.h\""
  "#include \"ShadowTree.h\"\n#include <cstdlib>")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/mounting/ShadowTree.cpp"
  "      return \"ReactRevisionMerge\";\n  }\n}"
  "      return \"ReactRevisionMerge\";\n  }\n  std::abort();\n}")

# Vector indices use the container size type for both interpolation paths.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/animated/nodes/InterpolationAnimatedNode.cpp"
  "double InterpolationAnimatedNode::interpolateValue(double value) {\n  // Compute range index\n  int index = 1;"
  "double InterpolationAnimatedNode::interpolateValue(double value) {\n  // Compute range index\n  decltype(inputRanges_)::size_type index = 1;")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/animated/nodes/InterpolationAnimatedNode.cpp"
  "double InterpolationAnimatedNode::interpolateColor(double value) {\n  // Compute range index\n  int index = 1;"
  "double InterpolationAnimatedNode::interpolateColor(double value) {\n  // Compute range index\n  decltype(inputRanges_)::size_type index = 1;")

# The observer state type is a class member and must have one header identity.

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/observers/intersection/IntersectionObserverState.h"
  "namespace {\nenum class IntersectionObserverStateType {\n  Initial,\n  NotIntersecting,\n  Intersecting,\n};\n}\n\nnamespace facebook::react {"
  "namespace facebook::react {\n\nenum class IntersectionObserverStateType {\n  Initial,\n  NotIntersecting,\n  Intersecting,\n};")

# These modern sources use no legacy JSExecutor declarations. Avoid importing
# the obsolete bridge and its incompatible mixed GNU/standard attributes.

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/observers/intersection/IntersectionObserverManager.cpp"
  "#include <cxxreact/JSExecutor.h>"
  "// Legacy JSExecutor declarations are not used by this translation unit.")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/uimanager/UIManager.cpp"
  "#include <cxxreact/JSExecutor.h>"
  "// Legacy JSExecutor declarations are not used by this translation unit.")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/nativemodule/webperformance/NativePerformance.cpp"
  "#include <cxxreact/JSExecutor.h>"
  "// Legacy JSExecutor declarations are not used by this translation unit.")

# NetworkIOAgent stores Stream in its header: use a named detail namespace
# shared by the declaration and definition instead of TU-local class types.

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/jsinspector-modern/NetworkIOAgent.h"
  "namespace {\nclass Stream; // Defined in NetworkIOAgent.cpp"
  "namespace network_io_detail {\nclass Stream; // Defined in NetworkIOAgent.cpp")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/jsinspector-modern/NetworkIOAgent.h"
  "} // namespace\n\nstruct LoadNetworkResourceRequest"
  "} // namespace network_io_detail\nusing network_io_detail::StreamsMap;\n\nstruct LoadNetworkResourceRequest")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/jsinspector-modern/NetworkIOAgent.cpp"
  "namespace {\n\nstruct InitStreamResult"
  "namespace network_io_detail {\n\nstruct InitStreamResult")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/jsinspector-modern/NetworkIOAgent.cpp"
  "} // namespace\n\nNetworkIOAgent::~NetworkIOAgent()"
  "} // namespace network_io_detail\nusing network_io_detail::InitStreamError;\nusing network_io_detail::InitStreamResult;\nusing network_io_detail::Stream;\n\nNetworkIOAgent::~NetworkIOAgent()")

# The retained deprecated compatibility overload must name its deprecated type.
# Scope GCC diagnostic handling to that implementation; callers still warn.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/nativemodule/core/ReactCommon/TurboModuleBinding.cpp"
  "void TurboModuleBinding::install(\n    jsi::Runtime& runtime,\n    TurboModuleProviderFunctionType&& moduleProvider,"
  "#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored \"-Wdeprecated-declarations\"\nvoid TurboModuleBinding::install(\n    jsi::Runtime& runtime,\n    TurboModuleProviderFunctionType&& moduleProvider,")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/nativemodule/core/ReactCommon/TurboModuleBinding.cpp"
  "      longLivedObjectCollection);\n}\n\nvoid TurboModuleBinding::install("
  "      longLivedObjectCollection);\n}\n#pragma GCC diagnostic pop\n\nvoid TurboModuleBinding::install(")

# Returning this local by name permits NRVO or the standard implicit move.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/bridging/Convert.h"
  "if constexpr (std::is_same_v<BaseT, jsi::Value>) {\n        return std::move(value);"
  "if constexpr (std::is_same_v<BaseT, jsi::Value>) {\n        return value;")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/attributedstring/AttributedStringBox.cpp"
  "#include <utility>"
  "#include <utility>\n#include <cstdlib>")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/attributedstring/AttributedStringBox.cpp"
  "      return lhs.getOpaquePointer() == rhs.getOpaquePointer();\n  }\n}"
  "      return lhs.getOpaquePointer() == rhs.getOpaquePointer();\n  }\n  std::abort();\n}")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/attributedstring/conversions.h"
  "#pragma once"
  "#pragma once\n\n#include <cstdlib>")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/attributedstring/conversions.h"
  "    case CSSFontVariant::NoContextual:\n      return std::nullopt;\n  }\n}"
  "    case CSSFontVariant::NoContextual:\n      return std::nullopt;\n  }\n  std::abort();\n}")

# GCC accepts both attributes in GNU spelling; preserve visibility and deprecation.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/ModuleRegistry.h"
  "class RN_EXPORT [[deprecated(\"This API will be removed along with the legacy architecture.\")]]"
  "class RN_EXPORT __attribute__((deprecated(\"This API will be removed along with the legacy architecture.\")))")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/RAMBundleRegistry.h"
  "class RN_EXPORT [[deprecated(\"This API will be removed along with the legacy architecture.\")]]"
  "class RN_EXPORT __attribute__((deprecated(\"This API will be removed along with the legacy architecture.\")))")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/Instance.h"
  "class RN_EXPORT [[deprecated(\"This API will be removed along with the legacy architecture.\")]]"
  "class RN_EXPORT __attribute__((deprecated(\"This API will be removed along with the legacy architecture.\")))")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/JSExecutor.h"
  "class RN_EXPORT [[deprecated(\"This API will be removed along with the legacy architecture.\")]]"
  "class RN_EXPORT __attribute__((deprecated(\"This API will be removed along with the legacy architecture.\")))")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/CxxNativeModule.h"
  "class RN_EXPORT [[deprecated(\"This API will be removed along with the legacy architecture.\")]]"
  "class RN_EXPORT __attribute__((deprecated(\"This API will be removed along with the legacy architecture.\")))")

# Initialize the packed header members instead of clearing a nontrivial object.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/JSBundleType.h"
  "  BundleHeader()\n  {\n    std::memset(this, 0, sizeof(BundleHeader));\n  }"
  "  BundleHeader() : magic64(0), version(0) {}")

# Validate nonnegative mapping offsets before comparing with an unsigned size.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/cxxreact/JSBigString.cpp"
  "CHECK(m_pageOff <= m_size)"
  "CHECK(m_pageOff >= 0 && static_cast<size_t>(m_pageOff) <= m_size)")

# Preserve size-domain loops and reject negative child indices before casting.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/uimanager/UIManagerUpdateShadowTree.cpp"
  "for (auto i = 0; i < size; i++)"
  "for (decltype(size) i = 0; i < size; i++)")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/react/renderer/uimanager/UIManagerUpdateShadowTree.cpp"
  "if (updatedChildIndex < children.size())"
  "if (updatedChildIndex >= 0 && static_cast<size_t>(updatedChildIndex) < children.size())")

# Continuation count is nonnegative; compare it in the vector length domain.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/jsinspector-modern/Utf8.h"
  "continuationBytes >= length - 1"
  "static_cast<std::vector<char>::size_type>(continuationBytes) >= length - 1")

rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/jsinspector-modern/RuntimeTargetConsole.cpp"
  "return std::move(retVal);"
  "return retVal;")

# The pointer parameter is local and unused afterward. Select vector's rvalue
# insertion path to avoid GCC's false positive in libc++ alias adjustment.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/yoga/yoga/node/Node.cpp"
  "children_.insert(children_.begin() + static_cast<ptrdiff_t>(index), child);"
  "children_.insert(children_.begin() + static_cast<ptrdiff_t>(index), std::move(child));")

# QNX uses POSIX process IDs and native pthread thread IDs.
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/oscompat/OSCompatPosix.cpp"
  "    defined(__ANDROID__)\n\n#include \"OSCompat.h\""
  "    defined(__ANDROID__) || defined(__QNXNTO__)\n\n#include \"OSCompat.h\"")
rnqnx_patch_qnx_source(
  "${REACT_COMMON_DIR}/oscompat/OSCompatPosix.cpp"
  "#elif defined(__linux__)\n\nuint64_t getCurrentThreadId()"
  "#elif defined(__QNXNTO__)\n\nuint64_t getCurrentThreadId() {\n  return static_cast<uint64_t>(pthread_self());\n}\n\n#elif defined(__linux__)\n\nuint64_t getCurrentThreadId()")
