#pragma once

// Private to the react-native-svg filter bridge (QtRnsvgFilterBridge.cpp and
// QtRnsvgFilterConvert*.cpp). Builds the retained filter model
// (native/qt/rnsvg/filter/FilterModel.h) from the generated Fabric props.

#include "QtRnsvgBridgeInternal.h"

#include <react/renderer/components/rnsvg/Props.h>

#include <QString>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace rnqnx::fabric::rnsvg_bridge {

// QtRnsvgFilterConvert.cpp: <filter> and the primitives the package mounts.
void appendFilterEntries(std::vector<Entry>& out);
// QtRnsvgFilterConvertExtras.cpp: the primitives packages/react-native-qnx/svg adds.
void appendFilterExtraEntries(std::vector<Entry>& out);

namespace filter_values {

// Bounds on what a prop can make the engine allocate or loop over. A prop
// outside them is not an error here: the values are cut to something the
// engine can validate, and the engine rejects a primitive whose numbers do not
// agree (a kernel of the wrong length, say).
constexpr int kMaxMatrixOrder = 4096;   // order * order fits int32
constexpr int kMaxOctaves = 64;         // amplitude halves per octave
constexpr size_t kMaxHrefBytes = 16 * 1024 * 1024;  // same limit as an <Image> data URI

/// `value`, or `fallback` for NaN and infinity.
inline double finiteOr(double value, double fallback = 0) { return std::isfinite(value) ? value : fallback; }

template <class T>
std::vector<double> numbers(const std::vector<T>& in) {
  std::vector<double> out;
  out.reserve(std::min(in.size(), kMaxListEntries));
  for (size_t i = 0; i < in.size() && i < kMaxListEntries; ++i) out.push_back(finiteOr(static_cast<double>(in[i])));
  return out;
}

inline QString text(const std::string& s) { return QString::fromStdString(s); }

template <class P, class = void>
struct HasIn1 : std::false_type {};
template <class P>
struct HasIn1<P, std::void_t<decltype(std::declval<const P&>().in1)>> : std::true_type {};

/// `in` as the primitive wrote it; empty is the previous result.
template <class P>
QString inputOf(const P& p) {
  if constexpr (HasIn1<P>::value) {
    return text(p.in1);
  } else {
    (void)p;
    return {};
  }
}

/// color-interpolation-filters as the generated enum's string: `auto` and
/// anything unknown are SVG's default, linearRGB.
template <class E>
filter::ColorSpace colorSpaceOf(const E& value) {
  return rn::toString(value) == "sRGB" ? filter::ColorSpace::SRGB : filter::ColorSpace::LinearRGB;
}

inline filter::EdgeMode edgeModeOf(const std::string& name) {
  if (name == "duplicate") return filter::EdgeMode::Duplicate;
  if (name == "wrap") return filter::EdgeMode::Wrap;
  return filter::EdgeMode::None;
}

/// The primitive with its x/y/width/height, result, input and color space.
template <class P>
std::shared_ptr<const filter::Primitive> makePrimitive(filter::PrimitiveType type, const P& p,
                                                       filter::Params params) {
  auto primitive = std::make_shared<filter::Primitive>();
  primitive->type = type;
  primitive->subregion = {toLength(p.x), toLength(p.y), toLength(p.width), toLength(p.height)};
  primitive->result = text(p.result);
  primitive->in1 = inputOf(p);
  primitive->colorSpace = colorSpaceOf(p.colorInterpolationFilters);
  primitive->params = std::move(params);
  return primitive;
}

/// A table row converter for one primitive: `Make` builds the parameters from
/// the generated props, the rest is the same for every primitive.
template <class P, filter::PrimitiveType Type, filter::Params (*Make)(const P&)>
bool convertPrimitive(const rn::Props& props, ElementProps& e, const ElementProps&) {
  const auto* p = dynamic_cast<const P*>(&props);
  if (!p) return false;
  e.kind = Kind::FilterPrimitive;
  e.primitive = makePrimitive(Type, *p, Make(*p));
  return true;
}

template <class P, filter::PrimitiveType Type, filter::Params (*Make)(const P&)>
Entry primitiveEntry(const char* name) {
  return Entry{name, Kind::FilterPrimitive, &convertPrimitive<P, Type, Make>, false};
}

/// A flood color, lighting color or shadow color prop: `{type, payload}` or a
/// packed ARGB number. Null (prop unset) keeps `fallback`.
inline Brush brushOr(const folly::dynamic& d, const Brush& fallback) {
  return d.isNull() ? fallback : toBrush(d);
}

inline double unitInterval(double v) { return std::clamp(finiteOr(v, 1.0), 0.0, 1.0); }

}  // namespace filter_values
}  // namespace rnqnx::fabric::rnsvg_bridge
