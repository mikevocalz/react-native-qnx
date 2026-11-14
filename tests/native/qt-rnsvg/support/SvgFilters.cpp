#include "SvgFilters.h"

#if RNQNX_RNSVG_FULL_MODEL

#include "PropBag.h"
#include "SvgCommonProps.h"
#include "SvgValues.h"

#include <QRegularExpression>

#include <cmath>
#include <memory>

namespace rnqnx::views::rnsvg::testing {
namespace {

namespace f = rnqnx::views::rnsvg::filter;

// extractFilter(): the subregion attributes as given; unset means the filter region.
f::Subregion subregionOf(const PropBag& p) {
  return {lengthProp(p, "x"), lengthProp(p, "y"), lengthProp(p, "width"), lengthProp(p, "height")};
}

ElementProps primitive(f::PrimitiveType type, f::Params params, f::Subregion subregion, const QString& result,
                       const QString& in1 = {}) {
  ElementProps e;
  e.kind = Kind::FilterPrimitive;
  auto prim = std::make_shared<f::Primitive>();
  prim->type = type;
  prim->subregion = subregion;
  prim->result = result;
  prim->in1 = in1;
  prim->params = std::move(params);
  e.primitive = std::move(prim);
  return e;
}

double number(const QString& text) {
  bool ok = false;
  const double v = text.trimmed().toDouble(&ok);
  return ok && std::isfinite(v) ? v : 0;
}

// FeColorMatrix `values`: split on whitespace, parseFloat each, drop NaN.
std::vector<double> matrixValues(const QString& text) {
  std::vector<double> out;
  for (const QString& token : text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
    qsizetype pos = 0;
    double v = 0;
    if (nextNumber(token, pos, v)) out.push_back(v);  // parseFloat reads a numeric prefix
  }
  return out;
}

f::Flood floodOf(const PropBag& p) {
  f::Flood flood;
  flood.color = Brush{Brush::Type::Color, QColor(Qt::black), {}};
  if (p.has(QStringLiteral("floodColor"))) {
    const QString v = p.get(QStringLiteral("floodColor"));
    if (!v.trimmed().isEmpty()) flood.color = parseBrush(v);
  }
  if (p.has(QStringLiteral("floodOpacity"))) flood.opacity = parseOpacity(p.get(QStringLiteral("floodOpacity")));
  return flood;
}

f::GaussianBlur blurOf(const PropBag& p) {
  f::GaussianBlur blur;
  const QString sd = p.get(QStringLiteral("stdDeviation")).trimmed();
  if (!sd.isEmpty()) {
    const QStringList parts = sd.split(QRegularExpression(QStringLiteral("\\s+")));
    blur.stdDeviationX = number(parts[0]);
    blur.stdDeviationY = parts.size() > 1 ? number(parts[1]) : number(parts[0]);
  }
  const QString edge = p.get(QStringLiteral("edgeMode"));
  if (edge == QLatin1String("duplicate")) blur.edgeMode = f::EdgeMode::Duplicate;
  else if (edge == QLatin1String("wrap")) blur.edgeMode = f::EdgeMode::Wrap;
  else if (edge == QLatin1String("none")) blur.edgeMode = f::EdgeMode::None;
  return blur;
}

// FeDropShadow.tsx renders blur(in), offset(result offsetblur), flood,
// composite(in2 offsetblur, operator in) and merge(result)(previous, in). An
// unset dx, dy or stdDeviation is 0 there, not the SVG default of 2.
void dropShadow(const PropBag& p, const QString& in1, std::vector<ElementProps>& out) {
  const QString source = in1.isEmpty() ? QStringLiteral("SourceGraphic") : in1;
  out.push_back(primitive(f::PrimitiveType::GaussianBlur, blurOf(p), {}, {}, source));
  f::Offset o;
  o.dx = lengthProp(p, "dx", lengthPx(0));
  o.dy = lengthProp(p, "dy", lengthPx(0));
  out.push_back(primitive(f::PrimitiveType::Offset, o, {}, QStringLiteral("offsetblur")));
  out.push_back(primitive(f::PrimitiveType::Flood, floodOf(p), {}, {}));
  f::Composite c;
  c.in2 = QStringLiteral("offsetblur");
  c.op = QStringLiteral("in");
  out.push_back(primitive(f::PrimitiveType::Composite, c, {}, {}));
  f::Merge merge;
  merge.nodes = {QString(), source};
  out.push_back(primitive(f::PrimitiveType::Merge, merge, {}, p.get(QStringLiteral("result"))));
}

}  // namespace

bool isFilterPrimitiveTag(const QString& tag) { return tag.startsWith(QLatin1String("fe")) && tag.size() > 2 && tag[2].isUpper(); }

std::vector<ElementProps> buildFilterPrimitives(const XmlNode& node) {
  const PropBag p(node);
  const QString in1 = p.get(QStringLiteral("in"));
  const f::Subregion sub = subregionOf(p);
  const QString result = p.get(QStringLiteral("result"));
  std::vector<ElementProps> out;
  const QString& tag = node.tag;
  if (tag == QLatin1String("feBlend")) {
    f::Blend b;
    b.in2 = p.get(QStringLiteral("in2"));
    b.mode = p.has(QStringLiteral("mode")) ? p.get(QStringLiteral("mode")) : QStringLiteral("normal");
    out.push_back(primitive(f::PrimitiveType::Blend, b, sub, result, in1));
  } else if (tag == QLatin1String("feColorMatrix")) {
    f::ColorMatrix m;
    m.type = p.has(QStringLiteral("type")) ? p.get(QStringLiteral("type")) : QStringLiteral("matrix");
    m.values = matrixValues(p.get(QStringLiteral("values")));
    out.push_back(primitive(f::PrimitiveType::ColorMatrix, m, sub, result, in1));
  } else if (tag == QLatin1String("feComposite")) {
    f::Composite c;
    c.in2 = p.get(QStringLiteral("in2"));
    c.op = p.has(QStringLiteral("operator")) ? p.get(QStringLiteral("operator")) : QStringLiteral("over");
    c.k1 = number(p.get(QStringLiteral("k1")));
    c.k2 = number(p.get(QStringLiteral("k2")));
    c.k3 = number(p.get(QStringLiteral("k3")));
    c.k4 = number(p.get(QStringLiteral("k4")));
    out.push_back(primitive(f::PrimitiveType::Composite, c, sub, result, in1));
  } else if (tag == QLatin1String("feFlood")) {
    out.push_back(primitive(f::PrimitiveType::Flood, floodOf(p), sub, result));
  } else if (tag == QLatin1String("feGaussianBlur")) {
    out.push_back(primitive(f::PrimitiveType::GaussianBlur, blurOf(p), sub, result, in1));
  } else if (tag == QLatin1String("feOffset")) {
    f::Offset o;
    o.dx = lengthProp(p, "dx", lengthPx(0));
    o.dy = lengthProp(p, "dy", lengthPx(0));
    out.push_back(primitive(f::PrimitiveType::Offset, o, sub, result, in1));
  } else if (tag == QLatin1String("feMerge")) {
    f::Merge merge;
    for (const auto& child : node.children) {
      const auto* n = std::get_if<std::unique_ptr<XmlNode>>(&child);
      if (n && (*n)->tag == QLatin1String("feMergeNode")) merge.nodes.push_back(PropBag(**n).get(QStringLiteral("in")));
    }
    out.push_back(primitive(f::PrimitiveType::Merge, merge, sub, result));
  } else if (tag == QLatin1String("feDropShadow")) {
    dropShadow(p, in1, out);
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::testing

#else  // !RNQNX_RNSVG_FULL_MODEL

namespace rnqnx::views::rnsvg::testing {

std::vector<ElementProps> buildFilterPrimitives(const XmlNode&) { return {}; }
bool isFilterPrimitiveTag(const QString&) { return false; }

}  // namespace rnqnx::views::rnsvg::testing

#endif
