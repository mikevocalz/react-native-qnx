#pragma once

// Value parsers for the fixture loader. They follow what react-native-svg's JS
// layer does before props reach the native side (src/lib/extract/*), plus the
// unit scaling the Fabric bridge applies to length strings.

#include "ModelAdapter.h"

#include <QString>
#include <QTransform>
#include <optional>
#include <vector>

namespace rnqnx::views::rnsvg::testing {

// A NumberProp string: "10", "50%", "1.5em", "2pt". Unset on garbage.
Length parseLength(const QString& text);

// A whitespace or comma separated list of lengths (extractLengthList).
std::vector<Length> parseLengthList(const QString& text);

// A number list, e.g. rotate="10 20".
std::vector<double> parseNumberList(const QString& text);

// processColor(): named colors, #rgb, #rgba, #rrggbb, #rrggbbaa, rgb(), rgba(),
// hsl(), hsla(), percentage channels, transparent. `nullopt` when invalid.
std::optional<QColor> parseColor(const QString& text);

// extractBrush(): none, currentColor, context-fill, context-stroke, url(#id), a color.
// An invalid color gives Brush::Type::None, as the JS layer's `null` does.
Brush parseBrush(const QString& text);

// extractTransform() for a string: translate, scale, rotate, skewX, skewY and
// matrix, separated by spaces or commas. `ok` is false on a syntax error, in
// which case the JS layer logs and uses the identity.
QTransform parseTransform(const QString& text, bool* ok = nullptr);

// extractOpacity(): numbers and percents, clamped to 0..1, NaN or above 1 -> 1.
double parseOpacity(const QString& text);

// url(#id) or #id -> id; empty when the text is neither (idPattern).
QString parseUrlId(const QString& text);

// extractViewBox() plus preserveAspectRatio.
struct ParsedViewBox {
  bool set{false};
  double minX{0}, minY{0}, width{0}, height{0};
  QString align{QStringLiteral("xMidYMid")};
  int meetOrSlice{0};
};
ParsedViewBox parseViewBox(const QString& viewBox, const QString& preserveAspectRatio);

// Reads one number from the front of `text` and advances `pos`; false at the end or on a non-number.
bool nextNumber(const QString& text, qsizetype& pos, double& out);

}  // namespace rnqnx::views::rnsvg::testing
