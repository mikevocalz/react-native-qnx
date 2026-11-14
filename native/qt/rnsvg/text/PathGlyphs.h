#pragma once

// Places one glyph on a textPath: where its midpoint lands, how it is
// rotated, and (for method=stretch) its outline warped onto the path.

#include "LayoutState.h"
#include "PathMeasure.h"

#include <QTransform>

namespace rnqnx::views::rnsvg::text::detail {

// A textPath element's geometry, measured once per layout.
struct PathRun {
  PathRun(const TextPathSpec& spec, bool& truncated);

  const TextPathSpec& spec;
  PathMeasure measure;
  double startOffset{0};  // user units along the (possibly reversed) path
};

struct PathGlyph {
  bool visible{false};
  QTransform frame;     // glyph-local pixels (x from the glyph start, y from the baseline) to user space
  QPainterPath outline; // user space
  QPointF origin;       // where the glyph start lands on the path
  double rotation{0};   // degrees, includes the per-character rotate
};

PathGlyph placeOnPath(const PathRun& run, const Unit& u, Anchor chunkAnchor);

}  // namespace rnqnx::views::rnsvg::text::detail
