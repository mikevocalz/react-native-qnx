#include "SvgGradient.h"

#include "PropBag.h"
#include "SvgCommonProps.h"
#include "SvgValues.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::testing {
namespace {

// percentToFloat(): "50%" -> 0.5, "0.3" -> 0.3, anything else warns and gives 0.
double offsetOf(const QString& raw) {
  static const QRegularExpression re(QStringLiteral("^([+-]?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)(%?)$"));
  const auto m = re.match(raw.trimmed());
  if (!m.hasMatch()) return 0;
  const double v = m.captured(1).toDouble();
  return m.captured(2).isEmpty() ? v : v / 100.0;
}

void collectStops(ElementProps& e, const XmlNode& gradient) {
  std::vector<std::pair<double, QColor>> stops;
  for (const auto& child : gradient.children) {
    const auto* node = std::get_if<std::unique_ptr<XmlNode>>(&child);
    if (!node || (*node)->tag != QLatin1String("stop")) continue;
    const PropBag s(**node);
    const double offset = offsetOf(s.has(QStringLiteral("offset")) ? s.get(QStringLiteral("offset")) : QStringLiteral("0"));
    const QString colorText = s.has(QStringLiteral("stopColor")) ? s.get(QStringLiteral("stopColor")) : QStringLiteral("#000");
    const auto color = parseColor(colorText);
    if (!color) continue;  // processColor() fails: the JS layer warns and skips the stop
    const double opacity = s.has(QStringLiteral("stopOpacity")) ? parseOpacity(s.get(QStringLiteral("stopOpacity"))) : 1.0;
    QColor c = *color;
    c.setAlpha(static_cast<int>(std::lround(opacity * 255)));  // the color's own alpha is replaced
    stops.push_back({offset, c});
  }
  std::stable_sort(stops.begin(), stops.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  double previous = 0;
  for (const auto& [offset, color] : stops) {
    previous = std::max(previous, offset);  // the bridge keeps offsets non-decreasing
    e.stops.push_back({previous, color});
  }
}

}  // namespace

ElementProps buildGradient(Kind kind, const XmlNode& node) {
  const PropBag p(node);
  ElementProps e;
  e.kind = kind;
  e.name = p.get(QStringLiteral("id"));
  e.gradientUserSpace = p.get(QStringLiteral("gradientUnits")) == QLatin1String("userSpaceOnUse");
  // extractTransform(gradientTransform || transform || props)
  e.gradientTransform = extractMatrix(p, p.has(QStringLiteral("gradientTransform")) ? "gradientTransform" : "transform");
  const QString spread = p.get(QStringLiteral("spreadMethod"));
#if RNQNX_RNSVG_FULL_MODEL
  e.spreadMethod = spread == QLatin1String("reflect") ? 1 : spread == QLatin1String("repeat") ? 2 : 0;
#else
  (void)spread;  // the old model has no spreadMethod
#endif
  if (kind == Kind::LinearGradient) {
    e.x1 = lengthProp(p, "x1", lengthPercent(0)); e.y1 = lengthProp(p, "y1", lengthPercent(0));
    e.x2 = lengthProp(p, "x2", lengthPercent(100)); e.y2 = lengthProp(p, "y2", lengthPercent(0));
  } else {
    e.cx = lengthProp(p, "cx", lengthPercent(50)); e.cy = lengthProp(p, "cy", lengthPercent(50));
    e.fx = p.has(QStringLiteral("fx")) ? parseLength(p.get(QStringLiteral("fx"))) : e.cx;
    e.fy = p.has(QStringLiteral("fy")) ? parseLength(p.get(QStringLiteral("fy"))) : e.cy;
    const Length r = lengthProp(p, "r", lengthPercent(50));
    // `rx || r`: a missing or zero rx falls back to r.
    const Length rx = lengthProp(p, "rx");
    const Length ry = lengthProp(p, "ry");
    e.rx = rx.set && rx.value != 0 ? rx : r;
    e.ry = ry.set && ry.value != 0 ? ry : r;
    e.r = r;
  }
  collectStops(e, node);
  return e;
}

}  // namespace rnqnx::views::rnsvg::testing
