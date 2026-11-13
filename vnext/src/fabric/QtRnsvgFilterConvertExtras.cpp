// The ten primitives react-native-svg declares in JavaScript and never mounts.
// packages/react-native-qnx/svg renders them as the Fabric components whose
// specs are in packages/react-native-qnx/svg/fabric; this file reads those
// props. The packages' JavaScript has already validated and defaulted them, so
// what is checked here is only what the native side cannot take on trust.

#include "QtRnsvgFilterConvert.h"

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

using namespace filter_values;
using filter::PrimitiveType;

filter::Params morphologyOf(const rn::RNSVGFeMorphologyProps& p) {
  filter::Morphology morphology;
  morphology.op = text(rn::toString(p.operator1));
  morphology.radiusX = toLength(p.radiusX);
  morphology.radiusY = toLength(p.radiusY);
  return morphology;
}

filter::Params turbulenceOf(const rn::RNSVGFeTurbulenceProps& p) {
  filter::Turbulence turbulence;
  turbulence.baseFrequencyX = finiteOr(p.baseFrequencyX);
  turbulence.baseFrequencyY = finiteOr(p.baseFrequencyY);
  turbulence.numOctaves = std::clamp(static_cast<int>(p.numOctaves), 0, kMaxOctaves);
  turbulence.seed = finiteOr(p.seed);
  turbulence.stitchTiles = p.stitchTiles == rn::RNSVGFeTurbulenceStitchTiles::Stitch;
  turbulence.type = text(rn::toString(p.type));
  return turbulence;
}

filter::Params displacementMapOf(const rn::RNSVGFeDisplacementMapProps& p) {
  filter::DisplacementMap map;
  map.in2 = text(p.in2);
  map.scale = finiteOr(p.scale);
  map.xChannelSelector = text(rn::toString(p.xChannelSelector));
  map.yChannelSelector = text(rn::toString(p.yChannelSelector));
  return map;
}

template <class S>
filter::TransferFunction transferFunctionOf(const S& s) {
  filter::TransferFunction f;
  // The struct's `type` is a plain string, empty when the prop was unset.
  if (!s.type.empty()) f.type = text(s.type);
  f.tableValues = numbers(s.tableValues);
  f.slope = finiteOr(s.slope, 1);
  f.intercept = finiteOr(s.intercept);
  f.amplitude = finiteOr(s.amplitude, 1);
  f.exponent = finiteOr(s.exponent, 1);
  f.offset = finiteOr(s.offset);
  return f;
}

filter::Params componentTransferOf(const rn::RNSVGFeComponentTransferProps& p) {
  filter::ComponentTransfer transfer;
  transfer.r = transferFunctionOf(p.funcR);
  transfer.g = transferFunctionOf(p.funcG);
  transfer.b = transferFunctionOf(p.funcB);
  transfer.a = transferFunctionOf(p.funcA);
  return transfer;
}

filter::Params dropShadowOf(const rn::RNSVGFeDropShadowProps& p) {
  filter::DropShadow shadow;
  // An unset offset keeps the model's SVG default of 2; the JavaScript sends
  // numbers, so this only matters for a host driven without it.
  if (const Length dx = toLength(p.dx); dx.set) shadow.dx = dx;
  if (const Length dy = toLength(p.dy); dy.set) shadow.dy = dy;
  shadow.stdDeviationX = finiteOr(p.stdDeviationX, 2);
  shadow.stdDeviationY = finiteOr(p.stdDeviationY, 2);
  shadow.color = brushOr(p.floodColor, shadow.color);
  shadow.opacity = unitInterval(p.floodOpacity);
  return shadow;
}

filter::Params imageOf(const rn::RNSVGFeImageProps& p) {
  filter::Image image;
  if (p.href.size() <= kMaxHrefBytes) image.href = text(p.href);
  if (!p.align.empty()) image.align = text(p.align);
  image.meetOrSlice = (p.meetOrSlice >= 0 && p.meetOrSlice <= 2) ? static_cast<int>(p.meetOrSlice) : 0;
  return image;
}

filter::Params convolveMatrixOf(const rn::RNSVGFeConvolveMatrixProps& p) {
  filter::ConvolveMatrix convolve;
  convolve.orderX = std::clamp(static_cast<int>(p.orderX), 0, kMaxMatrixOrder);
  convolve.orderY = std::clamp(static_cast<int>(p.orderY), 0, kMaxMatrixOrder);
  convolve.kernelMatrix = numbers(p.kernelMatrix);
  convolve.divisor = finiteOr(p.divisor);
  convolve.bias = finiteOr(p.bias);
  convolve.targetX = std::clamp(static_cast<int>(p.targetX), -1, kMaxMatrixOrder);
  convolve.targetY = std::clamp(static_cast<int>(p.targetY), -1, kMaxMatrixOrder);
  convolve.edgeMode = edgeModeOf(rn::toString(p.edgeMode));
  convolve.preserveAlpha = p.preserveAlpha;
  return convolve;
}

filter::Params tileOf(const rn::RNSVGFeTileProps&) { return filter::Tile{}; }

filter::LightSource lightOf(const std::string& kind, double azimuth, double elevation, double x, double y, double z,
                            double pointsAtX, double pointsAtY, double pointsAtZ, double specularExponent,
                            double limitingConeAngle, bool hasLimitingCone) {
  filter::LightSource light;
  if (kind == "point") {
    light.kind = filter::LightSource::Kind::Point;
  } else if (kind == "spot") {
    light.kind = filter::LightSource::Kind::Spot;
  } else {
    light.kind = filter::LightSource::Kind::Distant;
  }
  light.azimuth = finiteOr(azimuth);
  light.elevation = finiteOr(elevation);
  light.x = finiteOr(x);
  light.y = finiteOr(y);
  light.z = finiteOr(z);
  light.pointsAtX = finiteOr(pointsAtX);
  light.pointsAtY = finiteOr(pointsAtY);
  light.pointsAtZ = finiteOr(pointsAtZ);
  light.specularExponent = finiteOr(specularExponent, 1);
  light.limitingConeAngle = finiteOr(limitingConeAngle);
  light.hasLimitingCone = hasLimitingCone && std::isfinite(limitingConeAngle);
  return light;
}

template <class S>
filter::LightSource lightOf(const S& s) {
  return lightOf(rn::toString(s.kind), s.azimuth, s.elevation, s.x, s.y, s.z, s.pointsAtX, s.pointsAtY, s.pointsAtZ,
                 s.specularExponent, s.limitingConeAngle, s.hasLimitingCone);
}

// A lighting element with no light source child is in error in SVG, and
// browsers draw it as transparent black. The model has no "no light" state,
// so that result is a transparent flood.
template <class S>
bool hasLight(const S& s) {
  return rn::toString(s.kind) != "none";
}

filter::Params transparentBlack() {
  filter::Flood flood;
  flood.color = Brush{Brush::Type::Color, QColor(Qt::black), {}};
  flood.opacity = 0;
  return flood;
}

filter::Params diffuseLightingOf(const rn::RNSVGFeDiffuseLightingProps& p) {
  if (!hasLight(p.light)) return transparentBlack();
  filter::DiffuseLighting lighting;
  lighting.surfaceScale = finiteOr(p.surfaceScale, 1);
  lighting.diffuseConstant = finiteOr(p.diffuseConstant, 1);
  lighting.lightingColor = brushOr(p.lightingColor, lighting.lightingColor);
  lighting.light = lightOf(p.light);
  return lighting;
}

filter::Params specularLightingOf(const rn::RNSVGFeSpecularLightingProps& p) {
  if (!hasLight(p.light)) return transparentBlack();
  filter::SpecularLighting lighting;
  lighting.surfaceScale = finiteOr(p.surfaceScale, 1);
  lighting.specularConstant = finiteOr(p.specularConstant, 1);
  lighting.specularExponent = finiteOr(p.specularExponent, 1);
  lighting.lightingColor = brushOr(p.lightingColor, lighting.lightingColor);
  lighting.light = lightOf(p.light);
  return lighting;
}

// The primitive type goes with the parameters, so a lighting element that
// became a transparent flood has to say so.
template <class P, filter::PrimitiveType Type, filter::Params (*Make)(const P&)>
bool convertLighting(const rn::Props& props, ElementProps& e, const ElementProps&) {
  const auto* p = dynamic_cast<const P*>(&props);
  if (!p) return false;
  filter::Params params = Make(*p);
  const bool flood = std::holds_alternative<filter::Flood>(params);
  e.kind = Kind::FilterPrimitive;
  e.primitive = makePrimitive(flood ? PrimitiveType::Flood : Type, *p, std::move(params));
  return true;
}

}  // namespace

void appendFilterExtraEntries(std::vector<Entry>& out) {
  out.push_back(
      primitiveEntry<rn::RNSVGFeMorphologyProps, PrimitiveType::Morphology, morphologyOf>("RNSVGFeMorphology"));
  out.push_back(
      primitiveEntry<rn::RNSVGFeTurbulenceProps, PrimitiveType::Turbulence, turbulenceOf>("RNSVGFeTurbulence"));
  out.push_back(primitiveEntry<rn::RNSVGFeDisplacementMapProps, PrimitiveType::DisplacementMap, displacementMapOf>(
      "RNSVGFeDisplacementMap"));
  out.push_back(
      primitiveEntry<rn::RNSVGFeComponentTransferProps, PrimitiveType::ComponentTransfer, componentTransferOf>(
          "RNSVGFeComponentTransfer"));
  out.push_back(
      primitiveEntry<rn::RNSVGFeDropShadowProps, PrimitiveType::DropShadow, dropShadowOf>("RNSVGFeDropShadow"));
  out.push_back(primitiveEntry<rn::RNSVGFeImageProps, PrimitiveType::Image, imageOf>("RNSVGFeImage"));
  out.push_back(primitiveEntry<rn::RNSVGFeConvolveMatrixProps, PrimitiveType::ConvolveMatrix, convolveMatrixOf>(
      "RNSVGFeConvolveMatrix"));
  out.push_back(primitiveEntry<rn::RNSVGFeTileProps, PrimitiveType::Tile, tileOf>("RNSVGFeTile"));
  out.push_back(Entry{"RNSVGFeDiffuseLighting", Kind::FilterPrimitive,
                      &convertLighting<rn::RNSVGFeDiffuseLightingProps, PrimitiveType::DiffuseLighting,
                                       diffuseLightingOf>,
                      false});
  out.push_back(Entry{"RNSVGFeSpecularLighting", Kind::FilterPrimitive,
                      &convertLighting<rn::RNSVGFeSpecularLightingProps, PrimitiveType::SpecularLighting,
                                       specularLightingOf>,
                      false});
}

}  // namespace rnqnx::fabric::rnsvg_bridge
