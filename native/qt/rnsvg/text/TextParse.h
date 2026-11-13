#pragma once

// String forms of the text props, as react-native-svg's generated props carry
// them, mapped to the engine's enums. Unknown strings give the default.

#include "TextNode.h"

namespace rnqnx::views::rnsvg::text {

Anchor parseAnchor(const QString& s);
LengthAdjust parseLengthAdjust(const QString& s);
std::optional<AlignmentBaseline> parseAlignmentBaseline(const QString& s);
BaselineShift parseBaselineShiftKeyword(const QString& s);  // monostate unless a keyword
unsigned parseDecoration(const QString& s);
double parseStretch(const QString& s);  // font-stretch keyword to percent
std::vector<FontFeature> parseFeatureSettings(const QString& s);
PathSide parseSide(const QString& s);
PathMethod parseMethod(const QString& s);
PathSpacing parseSpacing(const QString& s);
PathMidLine parseMidLine(const QString& s);

}  // namespace rnqnx::views::rnsvg::text
