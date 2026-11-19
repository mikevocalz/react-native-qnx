#include "TextParse.h"

#include <QRegularExpression>
#include <QStringList>

namespace rnqnx::views::rnsvg::text {

namespace {

QString norm(const QString& s) { return s.trimmed().toLower(); }

}  // namespace

Anchor parseAnchor(const QString& s) {
  const QString v = norm(s);
  if (v == QLatin1String("middle")) return Anchor::Middle;
  if (v == QLatin1String("end")) return Anchor::End;
  return Anchor::Start;
}

LengthAdjust parseLengthAdjust(const QString& s) {
  return norm(s) == QLatin1String("spacingandglyphs") ? LengthAdjust::SpacingAndGlyphs : LengthAdjust::Spacing;
}

std::optional<AlignmentBaseline> parseAlignmentBaseline(const QString& s) {
  const QString v = norm(s);
  if (v.isEmpty() || v == QLatin1String("auto")) return std::nullopt;
  if (v == QLatin1String("baseline")) return AlignmentBaseline::Baseline;
  if (v == QLatin1String("alphabetic")) return AlignmentBaseline::Alphabetic;
  if (v == QLatin1String("ideographic")) return AlignmentBaseline::Ideographic;
  if (v == QLatin1String("middle")) return AlignmentBaseline::Middle;
  if (v == QLatin1String("central")) return AlignmentBaseline::Central;
  if (v == QLatin1String("mathematical")) return AlignmentBaseline::Mathematical;
  if (v == QLatin1String("hanging")) return AlignmentBaseline::Hanging;
  if (v == QLatin1String("text-top") || v == QLatin1String("text-before-edge") ||
      v == QLatin1String("before-edge")) {
    return AlignmentBaseline::TextTop;
  }
  if (v == QLatin1String("text-bottom") || v == QLatin1String("text-after-edge") ||
      v == QLatin1String("after-edge")) {
    return AlignmentBaseline::TextBottom;
  }
  if (v == QLatin1String("top")) return AlignmentBaseline::Top;
  if (v == QLatin1String("center")) return AlignmentBaseline::Center;
  if (v == QLatin1String("bottom")) return AlignmentBaseline::Bottom;
  return std::nullopt;
}

BaselineShift parseBaselineShiftKeyword(const QString& s) {
  const QString v = norm(s);
  if (v == QLatin1String("sub")) return ShiftKeyword::Sub;
  if (v == QLatin1String("super")) return ShiftKeyword::Super;
  if (v == QLatin1String("baseline")) return ShiftKeyword::Baseline;
  return std::monostate{};
}

unsigned parseDecoration(const QString& s) {
  unsigned flags = DecorationNone;
  const QStringList words = norm(s).split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString& w : words) {
    if (w == QLatin1String("underline")) flags |= DecorationUnderline;
    else if (w == QLatin1String("overline")) flags |= DecorationOverline;
    else if (w == QLatin1String("line-through") || w == QLatin1String("linethrough")) flags |= DecorationLineThrough;
  }
  return flags;
}

double parseStretch(const QString& s) {
  const QString v = norm(s);
  struct Entry { const char* name; double percent; };
  static const Entry table[] = {
      {"ultra-condensed", 50},  {"extra-condensed", 62.5}, {"condensed", 75},
      {"semi-condensed", 87.5}, {"normal", 100},           {"semi-expanded", 112.5},
      {"expanded", 125},        {"extra-expanded", 150},   {"ultra-expanded", 200},
  };
  for (const Entry& e : table) {
    if (v == QLatin1String(e.name)) return e.percent;
  }
  QString num = v;
  if (num.endsWith(QLatin1Char('%'))) num.chop(1);
  bool ok = false;
  const double p = num.toDouble(&ok);
  return ok && p > 0 && p < 1000 ? p : 100;
}

std::vector<FontFeature> parseFeatureSettings(const QString& s) {
  std::vector<FontFeature> out;
  const QStringList items = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
  for (const QString& item : items) {
    QString t = item.trimmed();
    if (t.isEmpty() || t == QLatin1String("normal")) continue;
    QString tag;
    QString rest;
    if (t.startsWith(QLatin1Char('"')) || t.startsWith(QLatin1Char('\''))) {
      const QChar quote = t.at(0);
      const qsizetype end = t.indexOf(quote, 1);
      if (end < 0) continue;
      tag = t.mid(1, end - 1);
      rest = t.mid(end + 1).trimmed();
    } else {
      const qsizetype cut = t.indexOf(QRegularExpression(QStringLiteral("[\\s=]")));
      tag = cut < 0 ? t : t.left(cut);
      rest = cut < 0 ? QString() : t.mid(cut + 1).trimmed();
    }
    if (tag.size() != 4) continue;
    unsigned value = 1;
    if (rest == QLatin1String("off")) value = 0;
    else if (!rest.isEmpty() && rest != QLatin1String("on")) {
      bool ok = false;
      const uint v = rest.toUInt(&ok);
      if (!ok) continue;
      value = v;
    }
    out.push_back({tag, value});
  }
  return out;
}

PathSide parseSide(const QString& s) {
  return norm(s) == QLatin1String("right") ? PathSide::Right : PathSide::Left;
}

PathMethod parseMethod(const QString& s) {
  return norm(s) == QLatin1String("stretch") ? PathMethod::Stretch : PathMethod::Align;
}

PathSpacing parseSpacing(const QString& s) {
  return norm(s) == QLatin1String("exact") ? PathSpacing::Exact : PathSpacing::Auto;
}

PathMidLine parseMidLine(const QString& s) {
  return norm(s) == QLatin1String("sharp") ? PathMidLine::Sharp : PathMidLine::Smooth;
}

}  // namespace rnqnx::views::rnsvg::text
