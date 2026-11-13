#pragma once

// Shapes one node's own characters with Qt (HarfBuzz underneath): kerning,
// ligatures, mark attachment and bidirectional reordering come from Qt. The
// result is one entry per typographic character (glyph cluster), in visual
// left-to-right order, with outlines and advances in em units.

#include "FontProbe.h"

#include <QPainterPath>
#include <vector>

namespace rnqnx::views::rnsvg::text {

struct ShapedGlyph {
  int firstChar{0};   // logical index of the cluster's first UTF-16 unit in the string
  int charCount{1};   // units the cluster covers (a ligature covers more than one)
  double advance{0};  // em, with the stretch correction applied
  QPainterPath outline;  // em units, origin on the baseline, y down; empty for spaces
  bool space{false};
  bool wordSeparator{false};  // U+0020 and U+00A0: where word-spacing applies
};

// Shapes `text` with `probe`. The text is capped by the caller.
std::vector<ShapedGlyph> shapeText(const QString& text, const Probe& probe);

}  // namespace rnqnx::views::rnsvg::text
