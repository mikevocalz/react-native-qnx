#pragma once

// The SVG text layout engine. `layout` turns a tree of resolved text nodes
// into glyph geometry, grouped per input node, so the scene can fill and
// stroke each node's output with that node's own paint.

#include "TextNode.h"

#include <QPointF>
#include <QRectF>
#include <vector>

namespace rnqnx::views::rnsvg::text {

// One drawn glyph: where its origin ended up. Spaces are not listed.
struct Placement {
  QPointF origin;        // glyph origin on the baseline, user space
  double rotation{0};    // degrees, clockwise on screen
  double advance{0};     // advance after textLength scaling
  int charIndex{0};      // index of its first character in the whole <text>
};

struct NodeOutput {
  int id{0};
  QPainterPath glyphs;       // outlines, positioned and rotated
  QPainterPath decorations;  // underline, overline and line-through rectangles
  QRectF advanceBox;         // union of the em boxes of the node's characters
  std::vector<Placement> placements;
};

struct Result {
  std::vector<NodeOutput> nodes;  // document order; only nodes with own text
  bool truncated{false};          // a cap (glyphs, depth, path) cut the input

  const NodeOutput* find(int id) const;
};

constexpr int kMaxGlyphs = 20000;
constexpr int kMaxDepth = 32;
constexpr int kMaxPathSegments = 50000;

// `start` is the initial text position of the root. Never throws; hostile
// input (NaN, negative sizes, huge trees) yields a partial or empty Result.
Result layout(const Node& root, QPointF start);

}  // namespace rnqnx::views::rnsvg::text
