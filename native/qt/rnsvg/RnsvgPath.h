#pragma once

#include "RnsvgBasicTypes.h"

#include <QPainterPath>
#include <QString>
#include <vector>

namespace rnqnx::views::rnsvg {

// Parses SVG path data (M L H V C S Q T A Z, absolute and relative). Returns
// the path built up to the first malformed token, as the SVG error model
// requires; `ok` (when given) is false if anything was rejected. Bounded: more
// than kMaxPathTokens numbers/commands stops parsing.
inline constexpr int kMaxPathTokens = 100000;
//
// `vertices`, when given, receives the path's vertices as the data wrote them
// (an arc is one segment however many cubics Qt stores), with the directions
// of travel into and out of each, for markers.
QPainterPath parseSvgPath(const QString& data, bool* ok = nullptr, std::vector<PathVertex>* vertices = nullptr);

}  // namespace rnqnx::views::rnsvg
