#include "LayoutState.h"

#include <algorithm>
#include <map>

namespace rnqnx::views::rnsvg::text::detail {

void markChunkStarts(State& s) {
  for (size_t i = 0; i < s.units.size(); ++i) {
    Unit& u = s.units[i];
    if (u.fixed) {
      u.chunkStart = false;
      continue;
    }
    const bool onPath = u.pathNode >= 0;
    const Unit* prev = i > 0 ? &s.units[i - 1] : nullptr;
    // An absolute x (or y off the path) starts a text chunk, and so does the
    // first character of a textPath.
    u.chunkStart = !prev || prev->fixed || prev->pathNode != u.pathNode || !unspecified(u.x) ||
                   (!onPath && !unspecified(u.y));
  }
}

namespace {

// The characters whose adjustment this textLength element owns: the ones in
// its subtree that no nearer textLength element claims.
std::vector<int> members(const State& s, int nodeIndex) {
  const FlatNode& f = s.nodes[size_t(nodeIndex)];
  std::vector<int> out;
  for (int i = f.firstUnit; i < f.endUnit; ++i) {
    const Unit& u = s.units[size_t(i)];
    if (!u.fixed && s.nodes[size_t(u.node)].lengthNode == nodeIndex) out.push_back(i);
  }
  return out;
}

}  // namespace

// SVG 1.1 section 10.5: textLength sets the advance of an element's text. With
// `spacing` the difference is spread over the gaps between glyphs; with
// `spacingAndGlyphs` glyphs and gaps are scaled horizontally. A nested
// textLength element governs its own text; the outer element then adjusts the
// rest to the difference between its length and the inner ones.
void applyTextLength(State& s) {
  std::map<int, double> innerLength;
  for (size_t i = 0; i < s.nodes.size(); ++i) {
    const FlatNode& f = s.nodes[i];
    if (f.lengthNode != int(i)) continue;
    const int up = f.parent >= 0 ? s.nodes[size_t(f.parent)].lengthNode : -1;
    if (up >= 0) innerLength[up] += *f.node->textLength;
  }

  for (size_t i = 0; i < s.nodes.size(); ++i) {
    const FlatNode& f = s.nodes[i];
    if (f.lengthNode != int(i)) continue;
    const std::vector<int> m = members(s, int(i));
    if (m.empty()) continue;
    const double target = *f.node->textLength - innerLength[int(i)];

    double natural = 0;
    double dxSum = 0;
    for (size_t k = 0; k < m.size(); ++k) {
      const Unit& u = s.units[size_t(m[k])];
      natural += u.advance;
      if (k + 1 < m.size()) natural += u.spacing;
      if (k > 0 && !u.chunkStart) {
        natural += u.kern + u.dx;
        dxSum += u.dx;
      }
    }

    if (f.node->lengthAdjust == LengthAdjust::SpacingAndGlyphs) {
      const double base = natural - dxSum;
      if (base <= 0 || target - dxSum <= 0) continue;
      const double scale = (target - dxSum) / base;
      if (!std::isfinite(scale)) continue;
      for (int idx : m) s.units[size_t(idx)].scaleX = scale;
    } else if (m.size() >= 2) {
      const double extra = (target - natural) / double(m.size() - 1);
      if (!std::isfinite(extra)) continue;
      for (size_t k = 0; k + 1 < m.size(); ++k) s.units[size_t(m[k])].extra = extra;
    }
  }
}

}  // namespace rnqnx::views::rnsvg::text::detail
