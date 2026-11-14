#include "Decoration.h"

#include <algorithm>

namespace rnqnx::views::rnsvg::text::detail {

// Each line is centred on its font-metric offset (post table for the
// underline, OS/2 for the strike-through, the ascent line for the overline) and
// uses the post table's thickness. The rectangle spans the full width of the
// run, so the line is continuous across spaces and spacing.
QPainterPath decorationShape(unsigned flags, const FontMetrics& m, double size, double width) {
  QPainterPath shape;
  if (!(width > 0) || flags == DecorationNone) return shape;
  const double thickness = std::max(m.lineThickness * size, 0.01 * size);
  const auto line = [&](double center) { shape.addRect(QRectF(0, center - thickness / 2, width, thickness)); };
  if (flags & DecorationUnderline) line(m.underlinePos * size);
  if (flags & DecorationOverline) line(-m.ascent * size);
  if (flags & DecorationLineThrough) line(-m.strikePos * size);
  return shape;
}

}  // namespace rnqnx::views::rnsvg::text::detail
