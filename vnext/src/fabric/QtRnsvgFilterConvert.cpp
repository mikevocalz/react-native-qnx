// <filter> and the seven primitives react-native-svg mounts itself: Blend,
// ColorMatrix, Composite, Flood, GaussianBlur, Merge, Offset.

#include "QtRnsvgFilterConvert.h"

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

using namespace filter_values;
using filter::PrimitiveType;

bool convertFilterElement(const rn::Props& props, ElementProps& e, const ElementProps&) {
  const auto* p = dynamic_cast<const rn::RNSVGFilterProps*>(&props);
  if (!p) return false;
  e.kind = Kind::Filter;
  e.name = text(p->name);
  e.x = toLength(p->x);
  e.y = toLength(p->y);
  e.width = toLength(p->width);
  e.height = toLength(p->height);
  e.filterUserSpace = p->filterUnits == rn::RNSVGFilterFilterUnits::UserSpaceOnUse;
  e.primitiveUserSpace = p->primitiveUnits == rn::RNSVGFilterPrimitiveUnits::UserSpaceOnUse;
  return true;
}

filter::Params blendOf(const rn::RNSVGFeBlendProps& p) {
  filter::Blend blend;
  blend.in2 = text(p.in2);
  const std::string mode = rn::toString(p.mode);
  // `unknown` is the generated enum's zero value, not an SVG keyword.
  blend.mode = mode == "unknown" ? QStringLiteral("normal") : text(mode);
  return blend;
}

filter::Params colorMatrixOf(const rn::RNSVGFeColorMatrixProps& p) {
  filter::ColorMatrix matrix;
  matrix.type = text(rn::toString(p.type));
  matrix.values = numbers(p.values);
  return matrix;
}

filter::Params compositeOf(const rn::RNSVGFeCompositeProps& p) {
  filter::Composite composite;
  composite.in2 = text(p.in2);
  composite.op = text(rn::toString(p.operator1));
  composite.k1 = finiteOr(p.k1);
  composite.k2 = finiteOr(p.k2);
  composite.k3 = finiteOr(p.k3);
  composite.k4 = finiteOr(p.k4);
  return composite;
}

filter::Params floodOf(const rn::RNSVGFeFloodProps& p) {
  filter::Flood flood;
  flood.color = brushOr(p.floodColor, flood.color);
  flood.opacity = unitInterval(p.floodOpacity);
  return flood;
}

filter::Params gaussianBlurOf(const rn::RNSVGFeGaussianBlurProps& p) {
  filter::GaussianBlur blur;
  blur.stdDeviationX = finiteOr(p.stdDeviationX);
  blur.stdDeviationY = finiteOr(p.stdDeviationY);
  blur.edgeMode = edgeModeOf(rn::toString(p.edgeMode));
  return blur;
}

// feMergeNode is not a component: the package sends each node's `in` as one
// entry of `nodes` (extractFeMerge), an empty string meaning the previous result.
filter::Params mergeOf(const rn::RNSVGFeMergeProps& p) {
  filter::Merge merge;
  const size_t count = std::min(p.nodes.size(), kMaxListEntries);
  merge.nodes.reserve(count);
  for (size_t i = 0; i < count; ++i) merge.nodes.push_back(text(p.nodes[i]));
  return merge;
}

filter::Params offsetOf(const rn::RNSVGFeOffsetProps& p) {
  filter::Offset offset;
  offset.dx = toLength(p.dx);
  offset.dy = toLength(p.dy);
  return offset;
}

}  // namespace

void appendFilterEntries(std::vector<Entry>& out) {
  out.push_back(Entry{"RNSVGFilter", Kind::Filter, &convertFilterElement, false});
  out.push_back(primitiveEntry<rn::RNSVGFeBlendProps, PrimitiveType::Blend, blendOf>("RNSVGFeBlend"));
  out.push_back(
      primitiveEntry<rn::RNSVGFeColorMatrixProps, PrimitiveType::ColorMatrix, colorMatrixOf>("RNSVGFeColorMatrix"));
  out.push_back(
      primitiveEntry<rn::RNSVGFeCompositeProps, PrimitiveType::Composite, compositeOf>("RNSVGFeComposite"));
  out.push_back(primitiveEntry<rn::RNSVGFeFloodProps, PrimitiveType::Flood, floodOf>("RNSVGFeFlood"));
  out.push_back(primitiveEntry<rn::RNSVGFeGaussianBlurProps, PrimitiveType::GaussianBlur, gaussianBlurOf>(
      "RNSVGFeGaussianBlur"));
  out.push_back(primitiveEntry<rn::RNSVGFeMergeProps, PrimitiveType::Merge, mergeOf>("RNSVGFeMerge"));
  out.push_back(primitiveEntry<rn::RNSVGFeOffsetProps, PrimitiveType::Offset, offsetOf>("RNSVGFeOffset"));
}

}  // namespace rnqnx::fabric::rnsvg_bridge
