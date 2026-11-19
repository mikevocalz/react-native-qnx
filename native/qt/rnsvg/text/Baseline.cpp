#include "LayoutState.h"
#include "TextParse.h"

#include <QStringList>
#include <algorithm>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

// A baseline-shift length: a number with an optional unit, resolved like
// react-native-svg's PropHelper (em and percent refer to the font size).
std::optional<double> parseShiftLength(const QString& token, double fontSize) {
  QString t = token.trimmed();
  if (t.isEmpty()) return std::nullopt;
  double unit = 1;
  const auto strip = [&](const char* suffix, double factor) {
    if (t.endsWith(QLatin1String(suffix))) {
      t.chop(int(qstrlen(suffix)));
      unit = factor;
      return true;
    }
    return false;
  };
  if (!(strip("px", 1) || strip("em", fontSize) || strip("ex", fontSize / 2) || strip("pt", 1.25) ||
        strip("pc", 15) || strip("mm", 3.543307) || strip("cm", 35.43307) || strip("in", 90) ||
        strip("%", fontSize / 100))) {
    unit = 1;
  }
  bool ok = false;
  const double v = t.toDouble(&ok);
  if (!ok || !std::isfinite(v)) return std::nullopt;
  return v * unit;
}

}  // namespace

void ownBaselineProps(const Node& node, std::optional<AlignmentBaseline>& baseline, BaselineShift& shift) {
  baseline = node.alignmentBaseline;
  shift = node.baselineShift;
  const QString va = node.verticalAlign.trimmed();
  if (va.isEmpty()) return;
  // Android's TextView splits at the last space: "<baseline-shift> <alignment-baseline>".
  // A lone token is taken as an alignment keyword if it is one, else as a shift.
  const qsizetype cut = va.lastIndexOf(QLatin1Char(' '));
  const QString shiftToken = cut < 0 ? QString() : va.left(cut).trimmed();
  const QString baselineToken = cut < 0 ? va : va.mid(cut + 1).trimmed();
  std::optional<AlignmentBaseline> parsedBaseline = parseAlignmentBaseline(baselineToken);
  QString shiftText = shiftToken;
  if (!parsedBaseline && cut < 0) shiftText = va;
  if (parsedBaseline && !baseline) baseline = parsedBaseline;
  if (!shiftText.isEmpty() && std::holds_alternative<std::monostate>(shift)) {
    BaselineShift keyword = parseBaselineShiftKeyword(shiftText);
    if (!std::holds_alternative<std::monostate>(keyword)) {
      shift = keyword;
    } else if (const auto v = parseShiftLength(shiftText, node.font.size)) {
      shift = *v;
    }
  }
}

// Offsets follow react-native-svg's TSpan: the value is added to y, so a
// positive result moves the glyphs down. Font metrics replace the CoreText and
// Paint metrics upstream reads.
double baselineYOffset(AlignmentBaseline baseline, const BaselineShift& shift, const FontMetrics& m, double size) {
  const double ascent = m.ascent * size;
  const double descent = m.descent * size;
  const double bottom = descent + m.leading * size;
  const double top = ascent;
  double offset = 0;
  switch (baseline) {
    case AlignmentBaseline::Baseline:
    case AlignmentBaseline::Alphabetic: offset = 0; break;
    case AlignmentBaseline::TextBottom:
    case AlignmentBaseline::Ideographic: offset = -descent; break;
    case AlignmentBaseline::Middle: offset = m.xHeight * size / 2; break;
    case AlignmentBaseline::Central: offset = (ascent - descent) / 2; break;
    case AlignmentBaseline::Mathematical: offset = 0.5 * ascent; break;
    case AlignmentBaseline::Hanging: offset = 0.8 * ascent; break;
    case AlignmentBaseline::TextTop: offset = ascent; break;
    case AlignmentBaseline::Bottom: offset = bottom; break;
    case AlignmentBaseline::Center: offset = (top + bottom) / 2; break;
    case AlignmentBaseline::Top: offset = top; break;
  }
  // baseline-shift does not apply with top or bottom alignment.
  if (baseline == AlignmentBaseline::Top || baseline == AlignmentBaseline::Bottom) return offset;
  if (const double* distance = std::get_if<double>(&shift)) {
    if (std::isfinite(*distance)) offset -= *distance;
  } else if (const ShiftKeyword* keyword = std::get_if<ShiftKeyword>(&shift)) {
    if (*keyword == ShiftKeyword::Sub) offset += size * m.subscriptDrop;
    else if (*keyword == ShiftKeyword::Super) offset -= size * m.superscriptRise;
  }
  return offset;
}

}  // namespace rnqnx::views::rnsvg::text::detail
