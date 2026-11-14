#pragma once

// text-decoration rectangles, in glyph-local pixels: x from the start of the
// run, y from the baseline (down is positive).

#include "FontProbe.h"
#include "TextNode.h"

#include <QPainterPath>

namespace rnqnx::views::rnsvg::text::detail {

QPainterPath decorationShape(unsigned flags, const FontMetrics& metrics, double size, double width);

}  // namespace rnqnx::views::rnsvg::text::detail
