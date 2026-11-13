#include "QtRnsvgBridgeInternal.h"

#include <QStringList>

#include <cmath>
#include <cstdint>
#include <string_view>

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

constexpr double kDefaultFontSize = 12.0;  // react-native-svg's default

bool isFiniteNumber(const folly::dynamic& d) { return d.isNumber() && std::isfinite(d.asDouble()); }

// em against the element's own font size. A size inherited from an ancestor
// is not known here, so an element that sets only an em length and inherits its
// size reads the react-native-svg default.
std::optional<double> absoluteFrom(const folly::dynamic& d, double fontSize) {
  const Length l = toLength(d);
  if (!l.set || l.unit == Length::Unit::Percent) return std::nullopt;
  return l.unit == Length::Unit::Em ? l.value * fontSize : l.value;
}

QString stringOf(const folly::dynamic* d) {
  return d && d->isString() ? QString::fromStdString(d->getString()) : QString();
}

bool isBaselineShiftKeyword(const QString& s) {
  return s == QLatin1String("baseline") || s == QLatin1String("sub") || s == QLatin1String("super");
}

bool isAlignmentBaselineKeyword(const QString& s) {
  static const char* const keywords[] = {
      "auto",       "baseline",   "before-edge", "text-before-edge", "middle", "central",
      "after-edge", "text-after-edge", "ideographic", "alphabetic", "hanging", "mathematical",
      "top",        "center",     "bottom",      "text-top",        "text-bottom",
      "use-script", "no-change",  "reset-size"};
  for (const char* k : keywords) {
    if (s == QLatin1String(k)) return true;
  }
  return false;
}

void setBaselineShift(TextProps& t, const QString& text) {
  const QString s = text.trimmed();
  if (s.isEmpty()) return;
  if (isBaselineShiftKeyword(s)) {
    t.baselineShiftKeyword = s;
    return;
  }
  t.baselineShift = lengthFromString(s.toStdString());
}

}  // namespace

Length lengthFromString(const std::string& raw) {
  const QString text = QString::fromStdString(raw).trimmed();
  int n = 0;
  if (n < text.size() && (text[n] == '+' || text[n] == '-')) ++n;
  while (n < text.size() && (text[n].isDigit() || text[n] == '.')) ++n;
  if (n < text.size() && (text[n] == 'e' || text[n] == 'E') && n + 1 < text.size() &&
      (text[n + 1].isDigit() || text[n + 1] == '-' || text[n + 1] == '+')) {
    n += 2;
    while (n < text.size() && text[n].isDigit()) ++n;
  }
  bool numeric = false;
  const double v = text.left(n).toDouble(&numeric);  // QString parsing is locale independent
  if (!numeric || !std::isfinite(v)) return {};
  const std::string unitText = text.mid(n).toStdString();
  std::string_view unit(unitText);
  while (!unit.empty() && unit.front() == ' ') unit.remove_prefix(1);
  while (!unit.empty() && unit.back() == ' ') unit.remove_suffix(1);
  if (unit == "%") return Length{v, Length::Unit::Percent, true};
  if (unit == "em") return Length{v, Length::Unit::Em, true};
  double scale = 1;
  if (unit == "pt") scale = 4.0 / 3.0;
  else if (unit == "pc") scale = 16;
  else if (unit == "mm") scale = 96.0 / 25.4;
  else if (unit == "cm") scale = 96.0 / 2.54;
  else if (unit == "in") scale = 96;
  return Length{v * scale, Length::Unit::Px, true};
}

Length toLength(const folly::dynamic& d) {
  if (d.isNumber()) return isFiniteNumber(d) ? Length{d.asDouble(), Length::Unit::Px, true} : Length{};
  if (d.isString()) return lengthFromString(d.getString());
  return {};
}

std::vector<Length> toLengths(const folly::dynamic& d) {
  std::vector<Length> out;
  if (d.isArray()) {
    for (const auto& item : d) {
      if (out.size() >= kMaxListEntries) break;
      out.push_back(toLength(item));
    }
  } else if (d.isNumber() || d.isString()) {
    out.push_back(toLength(d));
  }
  return out;
}

std::vector<double> toNumbers(const folly::dynamic& d) {
  std::vector<double> out;
  for (const Length& l : toLengths(d)) out.push_back(l.set ? l.value : 0.0);
  return out;
}

QColor colorFromArgb(double packed) {
  // JavaScript hands over a signed 32-bit integer; reinterpret as ARGB.
  if (!std::isfinite(packed)) return QColor(Qt::black);
  const auto bits = static_cast<std::uint32_t>(static_cast<std::int64_t>(packed));
  return QColor::fromRgba(bits);
}

QColor colorFrom(const rn::SharedColor& c) {
  return QColor(rn::redFromColor(c), rn::greenFromColor(c), rn::blueFromColor(c), rn::alphaFromColor(c));
}

Brush toBrush(const folly::dynamic& d) {
  Brush b;
  if (d.isNumber()) {
    b.type = Brush::Type::Color;
    b.color = colorFromArgb(d.asDouble());
    return b;
  }
  if (!d.isObject()) return b;  // null: `none`
  int kind = -1;
  if (const auto* v = d.get_ptr("type"); v && v->isNumber()) kind = static_cast<int>(v->asInt());
  switch (kind) {
    case 0: {
      b.type = Brush::Type::Color;
      b.color = QColor(Qt::black);
      if (const auto* p = d.get_ptr("payload"); p && p->isNumber()) b.color = colorFromArgb(p->asDouble());
      break;
    }
    case 1:
      b.type = Brush::Type::Ref;
      if (const auto* r = d.get_ptr("brushRef"); r && r->isString()) b.ref = QString::fromStdString(r->getString());
      break;
    case 2: b.type = Brush::Type::CurrentColor; break;
    case 3: b.type = Brush::Type::ContextFill; break;
    case 4: b.type = Brush::Type::ContextStroke; break;
    default: break;
  }
  return b;
}

std::optional<int> toWeight(const folly::dynamic& d) {
  if (d.isNumber()) return isFiniteNumber(d) ? std::optional<int>(static_cast<int>(d.asDouble())) : std::nullopt;
  if (!d.isString()) return std::nullopt;
  const auto& s = d.getString();
  if (s == "bold" || s == "bolder") return 700;
  if (s == "normal") return 400;
  if (s == "lighter") return 300;
  bool numeric = false;
  const double v = QString::fromStdString(s).trimmed().toDouble(&numeric);
  if (numeric && std::isfinite(v)) return static_cast<int>(v);
  return std::nullopt;
}

void mergeFont(FontSpec& f, const folly::dynamic& font) {
  if (!font.isObject()) return;
  if (const auto* v = font.get_ptr("fontFamily"); v && v->isString()) f.family = QString::fromStdString(v->getString());
  if (const auto* v = font.get_ptr("fontSize"); v) {
    if (auto size = absoluteFrom(*v, kDefaultFontSize)) f.size = *size;
  }
  if (const auto* v = font.get_ptr("fontWeight"); v) {
    if (auto w = toWeight(*v)) f.weight = *w;
  }
  if (const auto* v = font.get_ptr("fontStyle"); v && v->isString()) {
    f.italic = v->getString() == "italic" || v->getString() == "oblique";
  }
  const double emBase = f.size.value_or(kDefaultFontSize);
  if (const auto* v = font.get_ptr("letterSpacing"); v) {
    if (auto s = absoluteFrom(*v, emBase)) f.letterSpacing = *s;
  }
  if (const auto* v = font.get_ptr("wordSpacing"); v) {
    if (auto s = absoluteFrom(*v, emBase)) f.wordSpacing = *s;
  }
  // `kerning: "auto"` is the default and carries no value to store.
  if (const auto* v = font.get_ptr("kerning"); v) {
    if (auto s = absoluteFrom(*v, emBase)) f.kerning = *s;
  }
  const auto textFor = [&](const char* key, QString& target) {
    if (const QString s = stringOf(font.get_ptr(key)); !s.isEmpty()) target = s;
  };
  textFor("textAnchor", f.anchor);
  textFor("textDecoration", f.decoration);
  textFor("fontVariantLigatures", f.variantLigatures);
  textFor("fontStretch", f.fontStretch);
  textFor("fontFeatureSettings", f.fontFeatureSettings);
  textFor("fontVariant", f.variant);
  textFor("fontVariationSettings", f.variationSettings);
}

void mergeFontProps(FontSpec& f, const folly::dynamic& fontSize, const folly::dynamic& fontWeight) {
  if (!fontSize.isNull()) {
    if (auto size = absoluteFrom(fontSize, kDefaultFontSize)) f.size = *size;
  }
  if (!fontWeight.isNull()) {
    if (auto w = toWeight(fontWeight)) f.weight = *w;
  }
}

QTransform toTransform(const std::vector<rn::Float>& m) {
  // Callers check size() >= 6. A non-finite entry would poison every point the
  // matrix touches, so it reads as the identity.
  for (size_t i = 0; i < 6; ++i) {
    if (!std::isfinite(static_cast<double>(m[i]))) return QTransform();
  }
  return QTransform(m[0], m[1], m[2], m[3], m[4], m[5]);
}

void fillBaselineProps(TextProps& t, const folly::dynamic& baselineShift, const std::string& alignmentBaseline,
                       const folly::dynamic& verticalAlign) {
  t.alignmentBaseline = QString::fromStdString(alignmentBaseline).trimmed();
  if (baselineShift.isString()) {
    setBaselineShift(t, QString::fromStdString(baselineShift.getString()));
  } else if (baselineShift.isNumber()) {
    t.baselineShift = toLength(baselineShift);
  }

  t.verticalAlign = toLength(verticalAlign);
  // `vertical-align` is the shorthand for `baseline-shift alignment-baseline`
  // (react-native-svg's Android view splits it the same way; its iOS view
  // ignores the prop). The longhands win when both are given.
  QString text;
  if (verticalAlign.isString()) text = QString::fromStdString(verticalAlign.getString()).trimmed();
  else if (verticalAlign.isNumber() && isFiniteNumber(verticalAlign)) text = QString::number(verticalAlign.asDouble(), 'g', 17);
  if (text.isEmpty()) return;
  const QStringList tokens = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (tokens.isEmpty()) return;
  const bool shiftFree = !t.baselineShift.set && t.baselineShiftKeyword.isEmpty();
  if (tokens.size() >= 2) {
    if (shiftFree) setBaselineShift(t, tokens.first());
    if (t.alignmentBaseline.isEmpty() && isAlignmentBaselineKeyword(tokens.last())) t.alignmentBaseline = tokens.last();
  } else if (isAlignmentBaselineKeyword(tokens.first()) && !isBaselineShiftKeyword(tokens.first())) {
    if (t.alignmentBaseline.isEmpty()) t.alignmentBaseline = tokens.first();
  } else if (shiftFree) {
    setBaselineShift(t, tokens.first());
  }
}

ViewBox makeViewBox(double minX, double minY, double width, double height, const std::string& align, int meetOrSlice) {
  ViewBox v;
  if (!(width > 0) || !(height > 0) || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(minX) ||
      !std::isfinite(minY)) {
    return v;  // react-native-svg omits the props when `viewBox` is absent or invalid
  }
  v.set = true;
  v.minX = minX;
  v.minY = minY;
  v.width = width;
  v.height = height;
  if (!align.empty()) v.align = QString::fromStdString(align);
  // extractViewBox() sends align "none" with meetOrSlice 0; the model keeps
  // "none" as its own mode.
  v.meetOrSlice = v.align == QLatin1String("none") ? 2 : (meetOrSlice >= 0 && meetOrSlice <= 2 ? meetOrSlice : 0);
  return v;
}

}  // namespace rnqnx::fabric::rnsvg_bridge
