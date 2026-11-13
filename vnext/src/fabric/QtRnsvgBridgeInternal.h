#pragma once

// Private to the react-native-svg bridge (QtRnsvgBridge*.cpp). Value
// conversion from folly::dynamic and React types, the prop groups every element
// shares, and the table row type. Nothing here is a public API.

#include <RnsvgModel.h>

#include <folly/dynamic.h>
#include <react/renderer/core/Props.h>
#include <react/renderer/graphics/Color.h>
#include <react/renderer/graphics/Float.h>

#include <QColor>
#include <QString>
#include <QTransform>

#include <algorithm>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace facebook::react {
class ComponentDescriptorProviderRegistry;
}

namespace rnqnx::fabric::rnsvg_bridge {

namespace rn = facebook::react;
using namespace rnqnx::views::rnsvg;

// A per-character list (x, y, dx, dy, rotate) or a dash array holds at most
// this many entries. No text worth positioning glyph by glyph is longer, and
// the cap keeps a hostile prop from allocating without bound.
constexpr size_t kMaxListEntries = 65536;

// ---- QtRnsvgBridgeValues.cpp ----------------------------------------------

Length lengthFromString(const std::string& raw);
Length toLength(const folly::dynamic& d);
std::vector<Length> toLengths(const folly::dynamic& d);
// A list of plain numbers (`rotate`); units and non-finite entries read as 0.
std::vector<double> toNumbers(const folly::dynamic& d);
QColor colorFromArgb(double packed);
QColor colorFrom(const rn::SharedColor& c);
Brush toBrush(const folly::dynamic& d);
std::optional<int> toWeight(const folly::dynamic& d);
// Reads the `font` object react-native-svg builds in extractFont().
void mergeFont(FontSpec& f, const folly::dynamic& font);
// Sets font size / weight from the element's own fontSize and fontWeight props.
void mergeFontProps(FontSpec& f, const folly::dynamic& fontSize, const folly::dynamic& fontWeight);
QTransform toTransform(const std::vector<rn::Float>& m);
// baselineShift, alignmentBaseline and verticalAlign as the Text props carry
// them. `verticalAlign` is folded into the other two (see the .cpp).
void fillBaselineProps(TextProps& t, const folly::dynamic& baselineShift, const std::string& alignmentBaseline,
                       const folly::dynamic& verticalAlign);
ViewBox makeViewBox(double minX, double minY, double width, double height, const std::string& align, int meetOrSlice);

// ---- prop groups ------------------------------------------------------------
// The generated prop classes share field names, not a base class, so these are
// templates. Each reads only fields every class that calls it declares.

template <class P>
void nodeCommon(ElementProps& e, const P& p) {
  e.name = QString::fromStdString(p.name);
  e.opacity = p.opacity;
  if (p.matrix.size() >= 6) e.matrix = toTransform(p.matrix);
  e.clipPath = QString::fromStdString(p.clipPath);
  e.mask = QString::fromStdString(p.mask);
  e.markerStart = QString::fromStdString(p.markerStart);
  e.markerMid = QString::fromStdString(p.markerMid);
  e.markerEnd = QString::fromStdString(p.markerEnd);
  // -1 unset, 0 evenodd, 1 nonzero. Anything else is a malformed prop.
  e.clipRule = (p.clipRule == 0 || p.clipRule == 1) ? p.clipRule : -1;
  e.responsible = p.responsible;
  e.displayNone = p.display == "none";
  e.pointerEvents = QString::fromStdString(p.pointerEvents);
}

template <class P>
void renderable(ElementProps& e, const P& p) {
  nodeCommon(e, p);
  if (rn::isColorMeaningful(p.color)) e.color = colorFrom(p.color);
  e.fill = toBrush(p.fill);
  e.stroke = toBrush(p.stroke);
  e.fillOpacity = p.fillOpacity;
  e.strokeOpacity = p.strokeOpacity;
  e.fillEvenOdd = p.fillRule == 0;
  e.strokeWidth = toLength(p.strokeWidth);
  e.strokeLinecap = p.strokeLinecap;
  e.strokeLinejoin = p.strokeLinejoin;
  e.strokeDasharray = toLengths(p.strokeDasharray);
  e.strokeDashoffset = p.strokeDashoffset;
  e.strokeMiterlimit = p.strokeMiterlimit;
  e.nonScalingStroke = p.vectorEffect == 1;
  e.filter = QString::fromStdString(p.filter);
  for (const auto& name : p.propList) e.propList.insert(QString::fromStdString(name));
}

template <class P>
void fontCommon(ElementProps& e, const P& p) {
  mergeFont(e.font, p.font);
  // fontSize and fontWeight can also arrive as their own props.
  mergeFontProps(e.font, p.fontSize, p.fontWeight);
}

// x, y, dx, dy, rotate, inlineSize, baselineShift, alignmentBaseline,
// verticalAlign: the props Text, TSpan and TextPath share.
template <class P>
void textCommon(ElementProps& e, const P& p) {
  TextProps& t = e.text;
  t.x = toLengths(p.x);
  t.y = toLengths(p.y);
  t.dx = toLengths(p.dx);
  t.dy = toLengths(p.dy);
  t.rotate = toNumbers(p.rotate);
  t.inlineSize = toLength(p.inlineSize);
  t.textLength = toLength(p.textLength);
  t.lengthAdjust = QString::fromStdString(p.lengthAdjust);
  fillBaselineProps(t, p.baselineShift, p.alignmentBaseline, p.verticalAlign);
}

// `spreadMethod` is generated onto the gradient props by scripts/qt-rnsvg-codegen.mjs.
// This reads it when the generated header has it and leaves the pad default
// when it does not, so the bridge builds before and after regeneration.
template <class P, class = void>
struct HasSpreadMethod : std::false_type {};
template <class P>
struct HasSpreadMethod<P, std::void_t<decltype(std::declval<const P&>().spreadMethod)>> : std::true_type {};

template <class P>
int spreadMethodOf(const P& p) {
  if constexpr (HasSpreadMethod<P>::value) {
    return (p.spreadMethod >= 0 && p.spreadMethod <= 2) ? static_cast<int>(p.spreadMethod) : 0;
  } else {
    (void)p;
    return 0;
  }
}

template <class P>
void gradientCommon(ElementProps& e, const P& p) {
  nodeCommon(e, p);
  e.gradientUserSpace = p.gradientUnits == 1;
  if (p.gradientTransform.size() >= 6) e.gradientTransform = toTransform(p.gradientTransform);
  e.spreadMethod = spreadMethodOf(p);
  double previous = 0;
  for (size_t i = 0; i + 1 < p.gradient.size(); i += 2) {
    previous = std::max(previous, p.gradient[i]);  // SVG: offsets never decrease
    e.stops.push_back({previous, colorFromArgb(p.gradient[i + 1])});
  }
}

// ---- the element table ------------------------------------------------------

using Converter = bool (*)(const rn::Props&, ElementProps&, const ElementProps& previous);

struct Entry {
  const char* name;
  Kind kind;
  Converter convert;
  // The generated event emitter has `onSvgLayout` for this component.
  bool emitsSvgLayout;
};

template <class P, class Fn>
bool convertAs(const rn::Props& props, ElementProps& e, const ElementProps& previous, Fn&& fn) {
  const auto* p = dynamic_cast<const P*>(&props);
  if (!p) return false;
  fn(*p, e, previous);
  return true;
}

// Each file appends its rows; QtRnsvgBridge.cpp assembles the table once.
void appendShapeEntries(std::vector<Entry>& out);        // QtRnsvgBridgeShapes.cpp
void appendDefinitionEntries(std::vector<Entry>& out);   // QtRnsvgBridgeDefs.cpp
void appendTextEntries(std::vector<Entry>& out);         // QtRnsvgBridgeText.cpp

// QtRnsvgBridgeShadowNodes.cpp: registers the descriptors of every component
// this file set converts, <Svg> included.
void addCoreDescriptors(rn::ComponentDescriptorProviderRegistry& registry);

}  // namespace rnqnx::fabric::rnsvg_bridge
