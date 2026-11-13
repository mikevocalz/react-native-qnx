#include "SvgElementBuilders.h"

#include "PropBag.h"
#include "SvgCommonProps.h"
#include "SvgGradient.h"
#include "SvgImageSource.h"
#include "SvgText.h"
#include "SvgValues.h"

#include <QRegularExpression>

namespace rnqnx::views::rnsvg::testing {
namespace {

Length numberOrZero(const PropBag& p, const char* name) { return lengthProp(p, name, lengthPx(0)); }

ElementProps shape(Kind kind, const PropBag& p) {
  ElementProps e;
  e.kind = kind;
  extractCommonProps(e, p);
  return e;
}

ElementProps nameOnly(Kind kind, const PropBag& p) {
  ElementProps e;
  e.kind = kind;
  e.name = p.get(QStringLiteral("id"));
  return e;
}

// Units: objectBoundingBox 0, userSpaceOnUse 1 (src/lib/units.ts).
int unitsOf(const QString& v) { return v == QLatin1String("userSpaceOnUse") ? 1 : 0; }

#if RNQNX_RNSVG_FULL_MODEL
void applyViewBox(ElementProps& e, const PropBag& p) {
  const ParsedViewBox vb = parseViewBox(p.get(QStringLiteral("viewBox")), p.get(QStringLiteral("preserveAspectRatio")));
  e.viewBox.set = vb.set;
  e.viewBox.minX = vb.minX;
  e.viewBox.minY = vb.minY;
  e.viewBox.width = vb.width;
  e.viewBox.height = vb.height;
  e.viewBox.align = vb.align;
  e.viewBox.meetOrSlice = vb.meetOrSlice;
}
#endif

}  // namespace

QString polyPointsToPath(const QString& points, bool close) {
  if (points.isEmpty()) return {};
  // extractPolyPoints(): the replace has no `g` flag and no capture, so it
  // rewrites only the first "<non-e char>-" pair, dropping that char.
  static const QRegularExpression firstMinus(QStringLiteral("[^eE]-"));
  QString joined = points;
  if (const auto m = firstMinus.match(joined); m.hasMatch()) joined.replace(m.capturedStart(), m.capturedLength(), QStringLiteral(" -"));
  static const QRegularExpression sep(QStringLiteral("(?:\\s+|\\s*,\\s*)"));
  return QStringLiteral("M") + joined.split(sep).join(QLatin1Char(' ')) + (close ? QStringLiteral("z") : QString());
}

std::optional<Kind> kindForTag(const QString& tag) {
  static const QHash<QString, Kind> table = {
      {"svg", Kind::Group},
      {"g", Kind::Group},
      {"defs", Kind::Defs},
      {"path", Kind::Path},
      {"polygon", Kind::Path},
      {"polyline", Kind::Path},
      {"circle", Kind::Circle},
      {"ellipse", Kind::Ellipse},
      {"rect", Kind::Rect},
      {"line", Kind::Line},
      {"linearGradient", Kind::LinearGradient},
      {"radialGradient", Kind::RadialGradient},
      {"clipPath", Kind::ClipPath},
      {"mask", Kind::Mask},
      {"text", Kind::Text},
      {"tspan", Kind::TSpan},
      {"use", Kind::Use},
      {"image", Kind::Image},
#if RNQNX_RNSVG_FULL_MODEL
      {"pattern", Kind::Pattern},
      {"symbol", Kind::Symbol},
      {"marker", Kind::Marker},
      {"textPath", Kind::TextPath},
      {"foreignObject", Kind::ForeignObject},
      {"filter", Kind::Filter},
#endif
  };
  const auto it = table.constFind(tag);
  if (it == table.constEnd()) return std::nullopt;
  return it.value();
}

ElementProps buildElementProps(Kind kind, const XmlNode& node) {
  const PropBag p(node);
  switch (kind) {
    case Kind::Group:
    case Kind::Defs:
    case Kind::ClipPath: {
      ElementProps e = kind == Kind::Defs ? nameOnly(kind, p) : shape(kind, p);
      if (kind == Kind::Group) e.font = extractFont(p);
      return e;
    }
    case Kind::Path: {
      ElementProps e = shape(kind, p);
      QString d = p.get(QStringLiteral("d"));
      if (node.tag == QLatin1String("polygon")) d = polyPointsToPath(p.get(QStringLiteral("points")), true);
      else if (node.tag == QLatin1String("polyline")) d = polyPointsToPath(p.get(QStringLiteral("points")), false);
      e.pathData = d;
      e.path = parsePathInto(e, d);
      return e;
    }
    case Kind::Circle: {
      ElementProps e = shape(kind, p);
      e.cx = numberOrZero(p, "cx"); e.cy = numberOrZero(p, "cy"); e.r = numberOrZero(p, "r");
      return e;
    }
    case Kind::Ellipse: {
      ElementProps e = shape(kind, p);
      e.cx = numberOrZero(p, "cx"); e.cy = numberOrZero(p, "cy");
      e.rx = numberOrZero(p, "rx"); e.ry = numberOrZero(p, "ry");
      return e;
    }
    case Kind::Rect: {
      ElementProps e = shape(kind, p);
      e.x = numberOrZero(p, "x"); e.y = numberOrZero(p, "y");
      e.width = numberOrZero(p, "width"); e.height = numberOrZero(p, "height");
      e.rx = lengthProp(p, "rx"); e.ry = lengthProp(p, "ry");
      return e;
    }
    case Kind::Line: {
      ElementProps e = shape(kind, p);
      e.x1 = numberOrZero(p, "x1"); e.y1 = numberOrZero(p, "y1");
      e.x2 = numberOrZero(p, "x2"); e.y2 = numberOrZero(p, "y2");
      return e;
    }
    case Kind::LinearGradient:
    case Kind::RadialGradient:
      return buildGradient(kind, node);
    case Kind::Mask: {
      ElementProps e = shape(kind, p);
      e.x = lengthProp(p, "x", lengthPercent(0)); e.y = lengthProp(p, "y", lengthPercent(0));
      e.width = lengthProp(p, "width", lengthPercent(100)); e.height = lengthProp(p, "height", lengthPercent(100));
      e.maskUserSpace = p.has(QStringLiteral("maskUnits")) ? unitsOf(p.get(QStringLiteral("maskUnits"))) == 1 : false;
      e.maskContentUserSpace = p.has(QStringLiteral("maskContentUnits")) ? unitsOf(p.get(QStringLiteral("maskContentUnits"))) == 1 : true;
      e.maskAlpha = p.get(QStringLiteral("maskType")) == QLatin1String("alpha");
      return e;
    }
    case Kind::Use: {
      ElementProps e = shape(kind, p);
      e.x = numberOrZero(p, "x"); e.y = numberOrZero(p, "y");
      e.width = numberOrZero(p, "width"); e.height = numberOrZero(p, "height");
      e.href = parseUrlId(p.has(QStringLiteral("href")) ? p.get(QStringLiteral("href")) : p.get(QStringLiteral("xlinkHref")));
      return e;
    }
    case Kind::Image: {
      ElementProps e = shape(kind, p);
      e.x = numberOrZero(p, "x"); e.y = numberOrZero(p, "y");
      e.width = numberOrZero(p, "width"); e.height = numberOrZero(p, "height");
      const ParsedViewBox mode = parseViewBox(QStringLiteral("0 0 1 1"), p.has(QStringLiteral("preserveAspectRatio")) ? p.get(QStringLiteral("preserveAspectRatio")) : QStringLiteral("xMidYMid meet"));
      e.imageAlign = mode.align;
      e.imageMeetOrSlice = mode.meetOrSlice;
      loadImageSource(e, p.has(QStringLiteral("href")) ? p.get(QStringLiteral("href")) : p.get(QStringLiteral("xlinkHref")));
      return e;
    }
    case Kind::Text:
    case Kind::TSpan:
      return buildTextElement(kind, node);
#if RNQNX_RNSVG_FULL_MODEL
    case Kind::TextPath:
      return buildTextElement(kind, node);
    case Kind::Pattern: {
      ElementProps e = nameOnly(kind, p);
      e.x = lengthProp(p, "x", lengthPercent(0)); e.y = lengthProp(p, "y", lengthPercent(0));
      e.width = lengthProp(p, "width", lengthPercent(100)); e.height = lengthProp(p, "height", lengthPercent(100));
      // extractTransform(patternTransform || transform || props)
      const char* src = p.has(QStringLiteral("patternTransform")) ? "patternTransform" : "transform";
      e.patternTransform = extractMatrix(p, src);
      e.patternUserSpace = unitsOf(p.get(QStringLiteral("patternUnits"))) == 1;
      e.patternContentUserSpace = p.has(QStringLiteral("patternContentUnits")) ? unitsOf(p.get(QStringLiteral("patternContentUnits"))) == 1 : true;
      applyViewBox(e, p);
      return e;
    }
    case Kind::Symbol: {
      ElementProps e = nameOnly(kind, p);
      applyViewBox(e, p);
      return e;
    }
    case Kind::Marker: {
      ElementProps e = nameOnly(kind, p);
      e.refX = lengthProp(p, "refX", lengthPx(0)); e.refY = lengthProp(p, "refY", lengthPx(0));
      e.markerWidth = lengthProp(p, "markerWidth", lengthPx(3)); e.markerHeight = lengthProp(p, "markerHeight", lengthPx(3));
      e.markerStrokeWidthUnits = p.get(QStringLiteral("markerUnits")) != QLatin1String("userSpaceOnUse");
      e.orient = p.has(QStringLiteral("orient")) ? p.get(QStringLiteral("orient")) : QStringLiteral("0");
      applyViewBox(e, p);
      return e;
    }
    case Kind::ForeignObject: {
      ElementProps e = shape(kind, p);
      e.x = numberOrZero(p, "x"); e.y = numberOrZero(p, "y");
      e.width = lengthProp(p, "width"); e.height = lengthProp(p, "height");
      return e;
    }
    case Kind::Filter: {
      ElementProps e = nameOnly(kind, p);
      e.x = lengthProp(p, "x", lengthPercent(-10)); e.y = lengthProp(p, "y", lengthPercent(-10));
      e.width = lengthProp(p, "width", lengthPercent(120)); e.height = lengthProp(p, "height", lengthPercent(120));
      e.filterUserSpace = p.get(QStringLiteral("filterUnits")) == QLatin1String("userSpaceOnUse");
      e.primitiveUserSpace = p.has(QStringLiteral("primitiveUnits")) ? p.get(QStringLiteral("primitiveUnits")) == QLatin1String("userSpaceOnUse") : true;
      return e;
    }
    case Kind::FilterPrimitive:
      break;
#endif
    default:
      break;
  }
  return nameOnly(kind, p);
}

}  // namespace rnqnx::views::rnsvg::testing
