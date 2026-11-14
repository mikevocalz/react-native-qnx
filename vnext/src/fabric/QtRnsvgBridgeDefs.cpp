// Table rows for the elements that define things other elements use or that
// establish a viewport: gradients, ClipPath, Mask, Pattern, Symbol, Marker
// and ForeignObject.

#include "QtRnsvgBridgeInternal.h"

#include <react/renderer/components/rnsvg/Props.h>

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

bool convertLinearGradient(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGLinearGradientProps>(
      props, e, previous, [](const rn::RNSVGLinearGradientProps& p, ElementProps& e, const ElementProps&) {
        gradientCommon(e, p);
        e.x1 = toLength(p.x1);
        e.y1 = toLength(p.y1);
        e.x2 = toLength(p.x2);
        e.y2 = toLength(p.y2);
      });
}

bool convertRadialGradient(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGRadialGradientProps>(
      props, e, previous, [](const rn::RNSVGRadialGradientProps& p, ElementProps& e, const ElementProps&) {
        gradientCommon(e, p);
        e.fx = toLength(p.fx);
        e.fy = toLength(p.fy);
        e.cx = toLength(p.cx);
        e.cy = toLength(p.cy);
        e.rx = toLength(p.rx);
        e.ry = toLength(p.ry);
      });
}

bool convertClipPath(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGClipPathProps>(
      props, e, previous, [](const rn::RNSVGClipPathProps& p, ElementProps& e, const ElementProps&) {
        renderable(e, p);
        fontCommon(e, p);
      });
}

bool convertMask(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGMaskProps>(props, e, previous, [](const rn::RNSVGMaskProps& p, ElementProps& e, const ElementProps&) {
    renderable(e, p);
    fontCommon(e, p);
    e.x = toLength(p.x);
    e.y = toLength(p.y);
    e.width = toLength(p.width);
    e.height = toLength(p.height);
    e.maskUserSpace = p.maskUnits == 1;
    e.maskContentUserSpace = p.maskContentUnits != 0;
    e.maskAlpha = p.maskType == 1;
  });
}

// Pattern.tsx sends the same matrix as `matrix` and `patternTransform`, and
// sends both units props as 0 (objectBoundingBox) or 1 (userSpaceOnUse).
bool convertPattern(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGPatternProps>(
      props, e, previous, [](const rn::RNSVGPatternProps& p, ElementProps& e, const ElementProps&) {
        renderable(e, p);
        fontCommon(e, p);
        e.x = toLength(p.x);
        e.y = toLength(p.y);
        e.width = toLength(p.width);
        e.height = toLength(p.height);
        e.patternUserSpace = p.patternUnits == 1;
        e.patternContentUserSpace = p.patternContentUnits != 0;
        if (p.patternTransform.size() >= 6) e.patternTransform = toTransform(p.patternTransform);
        e.viewBox = makeViewBox(p.minX, p.minY, p.vbWidth, p.vbHeight, p.align, p.meetOrSlice);
      });
}

bool convertSymbol(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGSymbolProps>(
      props, e, previous, [](const rn::RNSVGSymbolProps& p, ElementProps& e, const ElementProps&) {
        renderable(e, p);
        fontCommon(e, p);
        e.viewBox = makeViewBox(p.minX, p.minY, p.vbWidth, p.vbHeight, p.align, p.meetOrSlice);
      });
}

// Marker.tsx sends `orient` as String(orient): "auto", "auto-start-reverse" or
// an angle such as "45" or "0".
bool convertMarker(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGMarkerProps>(
      props, e, previous, [](const rn::RNSVGMarkerProps& p, ElementProps& e, const ElementProps&) {
        renderable(e, p);
        fontCommon(e, p);
        e.refX = toLength(p.refX);
        e.refY = toLength(p.refY);
        e.markerWidth = toLength(p.markerWidth);
        e.markerHeight = toLength(p.markerHeight);
        e.markerStrokeWidthUnits = p.markerUnits != "userSpaceOnUse";
        e.orient = QString::fromStdString(p.orient);
        e.viewBox = makeViewBox(p.minX, p.minY, p.vbWidth, p.vbHeight, p.align, p.meetOrSlice);
      });
}

// The box is x, y, width, height in the user space of the parent. Its React
// Native children are laid out by Yoga inside the item (see
// RnsvgForeignObjectItem); this row carries the box and the inherited paint.
bool convertForeignObject(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGForeignObjectProps>(
      props, e, previous, [](const rn::RNSVGForeignObjectProps& p, ElementProps& e, const ElementProps&) {
        renderable(e, p);
        fontCommon(e, p);
        e.x = toLength(p.x);
        e.y = toLength(p.y);
        e.width = toLength(p.width);
        e.height = toLength(p.height);
      });
}

}  // namespace

void appendDefinitionEntries(std::vector<Entry>& out) {
  out.push_back(Entry{"RNSVGLinearGradient", Kind::LinearGradient, &convertLinearGradient, false});
  out.push_back(Entry{"RNSVGRadialGradient", Kind::RadialGradient, &convertRadialGradient, false});
  out.push_back(Entry{"RNSVGClipPath", Kind::ClipPath, &convertClipPath, true});
  out.push_back(Entry{"RNSVGMask", Kind::Mask, &convertMask, true});
  out.push_back(Entry{"RNSVGPattern", Kind::Pattern, &convertPattern, false});
  out.push_back(Entry{"RNSVGSymbol", Kind::Symbol, &convertSymbol, false});
  out.push_back(Entry{"RNSVGMarker", Kind::Marker, &convertMarker, false});
  out.push_back(Entry{"RNSVGForeignObject", Kind::ForeignObject, &convertForeignObject, true});
}

}  // namespace rnqnx::fabric::rnsvg_bridge
