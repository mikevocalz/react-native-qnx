#include "LayoutState.h"

namespace rnqnx::views::rnsvg::text::detail {

namespace {

// Distance from a glyph's origin to the next glyph's origin on the same line.
double stepOf(const Unit& u) { return (u.advance + u.spacing) * u.scaleX + u.extra; }

}  // namespace

// Walks the units in document order, keeping the SVG "current text position".
// Off a path the position is a point; on a textPath it is a distance along the
// path plus an offset from it, where `x` and `dx` count along and `y` and `dy`
// count across (react-native-svg treats them the same way).
void placeUnits(State& s) {
  s.chunks.clear();
  double cx = s.start.x(), cy = s.start.y();
  double distance = 0, across = 0;
  int open = -1;  // index of the chunk being filled
  const auto closeChunk = [&](int at) {
    if (open >= 0) s.chunks[size_t(open)].end = at;
    open = -1;
  };

  size_t i = 0;
  while (i < s.units.size()) {
    Unit& u = s.units[i];
    if (u.fixed) {
      closeChunk(int(i));
      size_t j = i;
      while (j < s.units.size() && s.units[j].fixed && s.units[j].node == u.node) ++j;
      const QPointF next = placeWrapped(s, u.node, int(i), int(j), QPointF(cx, cy));
      cx = next.x();
      cy = next.y();
      i = j;
      continue;
    }

    const bool onPath = u.pathNode >= 0;
    const bool enteringPath = onPath && (i == 0 || s.units[i - 1].fixed || s.units[i - 1].pathNode != u.pathNode);
    if (u.chunkStart || open < 0) {
      closeChunk(int(i));
      Chunk c;
      c.first = int(i);
      c.anchor = s.nodes[size_t(u.node)].node->anchor;
      s.chunks.push_back(c);
      open = int(s.chunks.size()) - 1;
    }
    if (enteringPath) {
      distance = 0;
      across = 0;
    }

    if (!onPath) {
      if (!unspecified(u.x)) cx = u.x;
      if (!unspecified(u.y)) cy = u.y;
      cx += u.dx;
      cy += u.dy;
      u.pos = cx + u.kern * u.scaleX;
      u.perp = cy + u.baselineOffset;
      u.end = u.pos + stepOf(u);
      cx = u.end;
    } else {
      if (!unspecified(u.x)) distance = u.x;
      if (!unspecified(u.y)) across = u.y;
      distance += u.dx;
      across += u.dy;
      u.pos = distance + u.kern * u.scaleX;
      u.perp = across + u.baselineOffset;
      u.end = u.pos + stepOf(u);
      distance = u.end;
    }
    u.chunk = open;
    Chunk& c = s.chunks[size_t(open)];
    if (u.chunkStart || c.end == 0) c.startPos = u.pos;
    c.endPos = u.end;
    c.end = int(i) + 1;
    ++i;
  }
  closeChunk(int(s.units.size()));
}

// text-anchor moves a whole chunk, whatever spans it crosses, so the shift is
// taken from the chunk's total advance and the first character's anchor.
void anchorChunks(State& s) {
  for (const Chunk& c : s.chunks) {
    const double width = c.endPos - c.startPos;
    double shift = 0;
    if (c.anchor == Anchor::Middle) shift = -width / 2;
    else if (c.anchor == Anchor::End) shift = -width;
    if (shift == 0) continue;
    for (int i = c.first; i < c.end; ++i) {
      s.units[size_t(i)].pos += shift;
      s.units[size_t(i)].end += shift;
    }
  }
}

}  // namespace rnqnx::views::rnsvg::text::detail
