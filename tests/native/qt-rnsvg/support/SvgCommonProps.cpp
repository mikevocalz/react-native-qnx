#include "SvgCommonProps.h"

#include "SvgValues.h"

#include <QRegularExpression>

#include <cmath>

namespace rnqnx::views::rnsvg::testing {
namespace {

int capOf(const QString& v) {
  if (v == QLatin1String("round")) return 1;
  if (v == QLatin1String("square")) return 2;
  return 0;
}

int joinOf(const QString& v) {
  if (v == QLatin1String("round")) return 1;
  if (v == QLatin1String("bevel")) return 2;
  return 0;
}

std::optional<int> weightOf(const QString& raw) {
  const QString s = raw.trimmed();
  if (s.isEmpty()) return std::nullopt;
  if (s == QLatin1String("bold") || s == QLatin1String("bolder")) return 700;
  if (s == QLatin1String("normal")) return 400;
  if (s == QLatin1String("lighter")) return 300;
  qsizetype pos = 0;
  double v = 0;
  if (nextNumber(s, pos, v)) return static_cast<int>(v);
  return std::nullopt;
}

// extractSingleFontFamily(): the first of a comma list, without quotes.
QString firstFamily(const QString& list) {
  if (list.isEmpty()) return {};
  QString first = list.split(QRegularExpression(QStringLiteral("\\s*,\\s*"))).value(0);
  first.remove(QRegularExpression(QStringLiteral("^[\\s\"']*")));
  first.remove(QRegularExpression(QStringLiteral("[\\s\"']*$")));
  return first;
}

std::optional<double> sizeOf(const Length& l) {
  if (!l.set || l.unit == Length::Unit::Percent) return std::nullopt;
  return l.unit == Length::Unit::Em ? l.value * 12.0 : l.value;
}

// parseFontString() for the `font` shorthand.
void applyFontShorthand(FontSpec& f, const QString& font) {
  static const QRegularExpression re(
      QStringLiteral("^\\s*((?:(?:normal|bold|italic)\\s+)*)(?:(\\d+(?:\\.\\d+)?(?:%|px|em|pt|pc|mm|cm|in]))*(?:\\s*/.*?)?\\s+)?\\s*\"?([^\"]*)"),
      QRegularExpression::CaseInsensitiveOption);
  const auto m = re.match(font);
  if (!m.hasMatch()) return;
  f.weight = m.captured(1).contains(QStringLiteral("bold"), Qt::CaseSensitive) ? 700 : 400;
  f.italic = m.captured(1).contains(QStringLiteral("italic"), Qt::CaseSensitive);
  const QString size = m.captured(2);
  f.size = size.isEmpty() ? std::optional<double>(12.0) : sizeOf(parseLength(size));
  f.family = firstFamily(m.captured(3));
}

}  // namespace

Length lengthPx(double v) { return Length{v, Length::Unit::Px, true}; }
Length lengthPercent(double v) { return Length{v, Length::Unit::Percent, true}; }

Length lengthProp(const PropBag& p, const char* name, Length fallback) {
  const QString key = QLatin1String(name);
  return p.has(key) ? parseLength(p.get(key)) : fallback;
}

std::optional<QTransform> extractMatrix(const PropBag& p, const char* name) {
  const QString key = QLatin1String(name);
  if (!p.has(key)) return std::nullopt;
  bool ok = true;
  const QTransform t = parseTransform(p.get(key), &ok);
  return ok ? t : QTransform();
}

FontSpec extractFont(const PropBag& p) {
  FontSpec f;
  if (p.has(QStringLiteral("font"))) applyFontShorthand(f, p.get(QStringLiteral("font")));
  if (p.has(QStringLiteral("fontFamily"))) f.family = firstFamily(p.get(QStringLiteral("fontFamily")));
  if (p.has(QStringLiteral("fontSize"))) {
    if (const auto s = sizeOf(parseLength(p.get(QStringLiteral("fontSize"))))) f.size = s;
  }
  if (p.has(QStringLiteral("fontWeight"))) {
    if (const auto w = weightOf(p.get(QStringLiteral("fontWeight")))) f.weight = w;
  }
  if (p.has(QStringLiteral("fontStyle"))) {
    const QString s = p.get(QStringLiteral("fontStyle"));
    f.italic = s == QLatin1String("italic") || s == QLatin1String("oblique");
  }
  if (p.has(QStringLiteral("textAnchor"))) f.anchor = p.get(QStringLiteral("textAnchor"));
  if (p.has(QStringLiteral("letterSpacing"))) {
    const Length l = parseLength(p.get(QStringLiteral("letterSpacing")));
    if (l.set) f.letterSpacing = l.value;
  }
  if (p.has(QStringLiteral("textDecoration"))) f.decoration = p.get(QStringLiteral("textDecoration"));
#if RNQNX_RNSVG_FULL_MODEL
  if (p.has(QStringLiteral("wordSpacing"))) {
    const Length l = parseLength(p.get(QStringLiteral("wordSpacing")));
    if (l.set) f.wordSpacing = l.value;
  }
  if (p.has(QStringLiteral("kerning"))) {
    const Length l = parseLength(p.get(QStringLiteral("kerning")));
    if (l.set) f.kerning = l.value;
  }
  if (p.has(QStringLiteral("fontVariantLigatures"))) f.variantLigatures = p.get(QStringLiteral("fontVariantLigatures"));
  if (p.has(QStringLiteral("fontStretch"))) f.fontStretch = p.get(QStringLiteral("fontStretch"));
  if (p.has(QStringLiteral("fontFeatureSettings"))) f.fontFeatureSettings = p.get(QStringLiteral("fontFeatureSettings"));
#endif
  return f;
}

void extractCommonProps(ElementProps& e, const PropBag& p) {
  auto mark = [&](const char* name) { e.propList.insert(QLatin1String(name)); };

  // extractFill: unset fill is black but not "set on this element".
  e.fill = Brush{Brush::Type::Color, QColor(Qt::black), {}};
  if (p.has(QStringLiteral("fill"))) {
    mark("fill");
    const QString v = p.get(QStringLiteral("fill"));
    if (!v.trimmed().isEmpty()) e.fill = parseBrush(v);
  }
  if (p.has(QStringLiteral("fillOpacity"))) {
    mark("fillOpacity");
    e.fillOpacity = parseOpacity(p.get(QStringLiteral("fillOpacity")));
  }
  if (p.has(QStringLiteral("fillRule"))) {
    mark("fillRule");
    e.fillEvenOdd = p.get(QStringLiteral("fillRule")) == QLatin1String("evenodd");
  }

  // extractStroke
  if (p.has(QStringLiteral("stroke"))) {
    mark("stroke");
    e.stroke = parseBrush(p.get(QStringLiteral("stroke")));
  }
  if (p.has(QStringLiteral("strokeWidth"))) {
    mark("strokeWidth");
    e.strokeWidth = parseLength(p.get(QStringLiteral("strokeWidth")));
  }
  if (p.has(QStringLiteral("strokeOpacity"))) {
    mark("strokeOpacity");
    e.strokeOpacity = parseOpacity(p.get(QStringLiteral("strokeOpacity")));
  }
  const QString dash = p.get(QStringLiteral("strokeDasharray")).trimmed();
  const bool hasDash = !dash.isEmpty() && dash != QLatin1String("none");
  if (p.has(QStringLiteral("strokeDasharray"))) {
    mark("strokeDasharray");
    if (hasDash) {
      e.strokeDasharray = parseLengthList(dash);
      if (e.strokeDasharray.size() % 2 == 1) {
        const auto copy = e.strokeDasharray;
        e.strokeDasharray.insert(e.strokeDasharray.end(), copy.begin(), copy.end());
      }
    }
  }
  if (p.has(QStringLiteral("strokeDashoffset"))) {
    mark("strokeDashoffset");
    const double off = p.get(QStringLiteral("strokeDashoffset")).toDouble();
    e.strokeDashoffset = hasDash && off != 0 ? off : 0;
  }
  if (p.has(QStringLiteral("strokeLinecap"))) {
    mark("strokeLinecap");
    e.strokeLinecap = capOf(p.get(QStringLiteral("strokeLinecap")));
  }
  if (p.has(QStringLiteral("strokeLinejoin"))) {
    mark("strokeLinejoin");
    e.strokeLinejoin = joinOf(p.get(QStringLiteral("strokeLinejoin")));
  }
  if (p.has(QStringLiteral("strokeMiterlimit"))) {
    mark("strokeMiterlimit");
    const double v = p.get(QStringLiteral("strokeMiterlimit")).toDouble();
    e.strokeMiterlimit = v != 0 ? v : 4;
  }
  if (p.has(QStringLiteral("vectorEffect"))) {
    const QString v = p.get(QStringLiteral("vectorEffect"));
    e.nonScalingStroke = v == QLatin1String("non-scaling-stroke") || v == QLatin1String("nonScalingStroke");
  }

  if (p.has(QStringLiteral("color"))) {
    if (const auto c = parseColor(p.get(QStringLiteral("color")))) e.color = *c;
  }
  e.matrix = extractMatrix(p);
  if (p.has(QStringLiteral("opacity"))) e.opacity = parseOpacity(p.get(QStringLiteral("opacity")));
  e.displayNone = p.get(QStringLiteral("display")) == QLatin1String("none");

#if RNQNX_RNSVG_FULL_MODEL
  const QString marker = p.get(QStringLiteral("marker"));
  e.markerStart = parseUrlId(p.has(QStringLiteral("markerStart")) ? p.get(QStringLiteral("markerStart")) : marker);
  e.markerMid = parseUrlId(p.has(QStringLiteral("markerMid")) ? p.get(QStringLiteral("markerMid")) : marker);
  e.markerEnd = parseUrlId(p.has(QStringLiteral("markerEnd")) ? p.get(QStringLiteral("markerEnd")) : marker);
  e.filter = parseUrlId(p.get(QStringLiteral("filter")));
#endif
  e.name = p.get(QStringLiteral("id"));
  if (p.has(QStringLiteral("clipRule"))) {
    const QString r = p.get(QStringLiteral("clipRule"));
    if (!r.isEmpty()) setClipRule(e, r == QLatin1String("evenodd") ? 0 : 1);
  }
  e.clipPath = parseUrlId(p.get(QStringLiteral("clipPath")));
  e.mask = parseUrlId(p.get(QStringLiteral("mask")));
  e.pointerEvents = p.get(QStringLiteral("pointerEvents"));
}

}  // namespace rnqnx::views::rnsvg::testing
