#include "TextLayout.h"

#include "LayoutState.h"

namespace rnqnx::views::rnsvg::text {

const NodeOutput* Result::find(int id) const {
  for (const NodeOutput& n : nodes) {
    if (n.id == id) return &n;
  }
  return nullptr;
}

// Passes, in order: flatten the tree, resolve per-character lists, shape each
// node, find text chunks and apply textLength, walk the text position,
// anchor the chunks, then build the geometry. Each pass reads what the
// previous ones wrote into State.
Result layout(const Node& root, QPointF start) {
  Result result;
  if (!std::isfinite(start.x()) || !std::isfinite(start.y())) start = QPointF(0, 0);
  detail::State s;
  s.start = start;
  detail::flatten(root, s);
  detail::resolveCharAttrs(s);
  detail::buildUnits(s);
  detail::markChunkStarts(s);
  detail::applyTextLength(s);
  detail::placeUnits(s);
  detail::anchorChunks(s);
  detail::emitGeometry(s, result);
  result.truncated = s.truncated;
  return result;
}

}  // namespace rnqnx::views::rnsvg::text
