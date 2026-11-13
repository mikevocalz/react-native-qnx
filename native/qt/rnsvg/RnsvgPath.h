#pragma once

#include <QPainterPath>
#include <QString>

namespace rnqnx::views::rnsvg {

// Parses SVG path data (M L H V C S Q T A Z, absolute and relative). Returns
// the path built up to the first malformed token, as the SVG error model
// requires; `ok` (when given) is false if anything was rejected. Bounded: more
// than kMaxPathTokens numbers/commands stops parsing.
inline constexpr int kMaxPathTokens = 100000;
QPainterPath parseSvgPath(const QString& data, bool* ok = nullptr);

}  // namespace rnqnx::views::rnsvg
