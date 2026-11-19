#include "LayoutState.h"
#include "Shaper.h"

#include <algorithm>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

double finiteOr(double v, double fallback) { return std::isfinite(v) ? v : fallback; }

// A ligature joins characters into one glyph. When a later character of the
// cluster carries its own position, the glyph cannot stay joined (SVG 2:
// ligatures do not span characters that are positioned separately).
bool positionsInsideCluster(const std::vector<ShapedGlyph>& glyphs, const State& s, int ownFirstChar) {
  for (const ShapedGlyph& g : glyphs) {
    for (int k = 1; k < g.charCount; ++k) {
      const size_t c = size_t(ownFirstChar + g.firstChar + k);
      if (c >= size_t(s.totalChars)) continue;
      if (!unspecified(s.attrs.x[c]) || !unspecified(s.attrs.y[c]) || !unspecified(s.attrs.dx[c]) ||
          !unspecified(s.attrs.dy[c])) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void buildUnits(State& s) {
  s.units.clear();
  for (size_t i = 0; i < s.nodes.size(); ++i) {
    FlatNode& f = s.nodes[i];
    const Node& node = *f.node;
    f.firstUnit = f.ownFirstUnit = int(s.units.size());
    if (f.valid && !f.content.isEmpty()) {
      const double letterSpacing = finiteOr(node.font.letterSpacing, 0);
      const double wordSpacing = finiteOr(node.font.wordSpacing, 0);
      const bool ligatures = node.font.ligatures && letterSpacing == 0;
      const Probe* probe = &s.probes.get(node.font, ligatures);
      std::vector<ShapedGlyph> glyphs = shapeText(f.content, *probe);
      if (ligatures && positionsInsideCluster(glyphs, s, f.ownFirstChar)) {
        probe = &s.probes.get(node.font, false);
        glyphs = shapeText(f.content, *probe);
      }
      const bool wrapped = node.inlineSize && std::isfinite(*node.inlineSize) && *node.inlineSize > 0 &&
                           f.pathNode < 0;
      const double size = node.font.size;
      const double offset = baselineYOffset(f.baseline, f.shift, probe->metrics, size);
      const double kern = node.font.kerning ? finiteOr(*node.font.kerning, 0) : 0;
      for (ShapedGlyph& g : glyphs) {
        Unit u;
        u.node = int(i);
        u.charIndex = f.ownFirstChar + g.firstChar;
        u.charCount = g.charCount;
        u.size = size;
        u.advance = g.advance * size;
        u.outline = std::move(g.outline);
        u.space = g.space;
        u.wordSeparator = g.wordSeparator;
        u.spacing = letterSpacing + (g.wordSeparator ? wordSpacing : 0);
        u.kern = kern;
        u.baselineOffset = offset;
        const size_t c = size_t(u.charIndex);
        u.x = s.attrs.x[c];
        u.y = s.attrs.y[c];
        u.dx = finiteOr(s.attrs.dx[c], 0);
        u.dy = finiteOr(s.attrs.dy[c], 0);
        u.rotate = finiteOr(s.attrs.rotate[c], 0);
        u.pathNode = f.pathNode;
        u.fixed = wrapped;
        s.units.push_back(std::move(u));
      }
    }
    f.ownEndUnit = int(s.units.size());
  }
  // Document order means a subtree's units are contiguous.
  for (size_t i = 0; i < s.nodes.size(); ++i) {
    FlatNode& f = s.nodes[i];
    f.endUnit = f.subtreeEnd < int(s.nodes.size()) ? s.nodes[size_t(f.subtreeEnd)].firstUnit : int(s.units.size());
    f.firstUnit = f.ownFirstUnit;
  }
}

}  // namespace rnqnx::views::rnsvg::text::detail
