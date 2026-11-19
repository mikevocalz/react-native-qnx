#include "Style.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {

QColor withAlpha(QColor c, double a) {
  c.setAlphaF(std::clamp(c.alphaF() * a, 0.0, 1.0));
  return c;
}

void applyProps(Style& s, const ElementProps& p) {
  const auto& pl = p.propList;
  if (pl.contains(QStringLiteral("fill"))) s.fill = p.fill;
  if (pl.contains(QStringLiteral("fillOpacity"))) s.fillOpacity = p.fillOpacity;
  if (pl.contains(QStringLiteral("fillRule"))) s.fillEvenOdd = p.fillEvenOdd;
  if (pl.contains(QStringLiteral("stroke"))) s.stroke = p.stroke;
  if (pl.contains(QStringLiteral("strokeWidth")) && p.strokeWidth.set) s.strokeWidth = p.strokeWidth;
  if (pl.contains(QStringLiteral("strokeOpacity"))) s.strokeOpacity = p.strokeOpacity;
  if (pl.contains(QStringLiteral("strokeDasharray"))) s.dash = p.strokeDasharray;
  if (pl.contains(QStringLiteral("strokeDashoffset"))) s.dashOffset = p.strokeDashoffset;
  if (pl.contains(QStringLiteral("strokeLinecap"))) s.cap = p.strokeLinecap;
  if (pl.contains(QStringLiteral("strokeLinejoin"))) s.join = p.strokeLinejoin;
  if (pl.contains(QStringLiteral("strokeMiterlimit")) && p.strokeMiterlimit > 0) s.miter = p.strokeMiterlimit;
  if (p.nonScalingStroke) s.nonScaling = true;
  if (p.clipRule >= 0) s.clipRule = p.clipRule;
  if (p.color) s.color = *p.color;
  if (!p.font.family.isEmpty()) s.font.family = p.font.family;
  if (p.font.size) s.font.size = p.font.size;
  if (p.font.weight) s.font.weight = p.font.weight;
  if (p.font.italic) s.font.italic = p.font.italic;
  if (!p.font.anchor.isEmpty()) s.font.anchor = p.font.anchor;
  if (p.font.letterSpacing) s.font.letterSpacing = p.font.letterSpacing;
  if (p.font.wordSpacing) s.font.wordSpacing = p.font.wordSpacing;
  if (p.font.kerning) s.font.kerning = p.font.kerning;
  if (!p.font.decoration.isEmpty()) s.font.decoration = p.font.decoration;
  if (!p.font.variantLigatures.isEmpty()) s.font.variantLigatures = p.font.variantLigatures;
  if (!p.font.fontStretch.isEmpty()) s.font.fontStretch = p.font.fontStretch;
  if (!p.font.fontFeatureSettings.isEmpty()) s.font.fontFeatureSettings = p.font.fontFeatureSettings;
  if (!p.markerStart.isEmpty()) s.markerStart = p.markerStart;
  if (!p.markerMid.isEmpty()) s.markerMid = p.markerMid;
  if (!p.markerEnd.isEmpty()) s.markerEnd = p.markerEnd;
}

double Units::resolve(const Length& l, Axis axis, double fallback) const {
  if (!l.set) return fallback;
  switch (l.unit) {
    case Length::Unit::Percent: {
      const double w = viewport_.width(), h = viewport_.height();
      const double ref = axis == Axis::X ? w : axis == Axis::Y ? h : std::sqrt((w * w + h * h) / 2.0);
      return l.value / 100.0 * ref;
    }
    case Length::Unit::Em: return l.value * fontSize_;
    default: return l.value;
  }
}

double Units::resolveUnits(const Length& l, bool userSpace, Axis axis, double fallback) const {
  if (!l.set) return fallback;
  if (userSpace) return resolve(l, axis);
  return l.unit == Length::Unit::Percent ? l.value / 100.0 : l.value;
}

}  // namespace rnqnx::views::rnsvg
