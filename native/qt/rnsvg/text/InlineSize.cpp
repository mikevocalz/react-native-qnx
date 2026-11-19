#include "LayoutState.h"

#include <algorithm>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

struct Line {
  int first{0};
  int end{0};
  double width{0};  // without trailing spaces
  double full{0};   // with them
};

double stepOf(const Unit& u) { return u.kern + u.advance + u.spacing; }

// Greedy word wrap on spaces. A word wider than the box stays whole on a line
// of its own, as in the UILabel and StaticLayout react-native-svg draws with.
std::vector<Line> breakLines(const State& s, int first, int end, double limit) {
  std::vector<Line> lines;
  Line cur;
  cur.first = cur.end = first;
  int i = first;
  while (i < end) {
    int wordEnd = i;
    double wordWidth = 0;
    while (wordEnd < end && !s.units[size_t(wordEnd)].space) wordWidth += stepOf(s.units[size_t(wordEnd++)]);
    int spaceEnd = wordEnd;
    double spaceWidth = 0;
    while (spaceEnd < end && s.units[size_t(spaceEnd)].space) spaceWidth += stepOf(s.units[size_t(spaceEnd++)]);
    if (cur.end != cur.first && cur.full + wordWidth > limit) {
      lines.push_back(cur);
      cur = Line();
      cur.first = cur.end = i;
    }
    cur.width = cur.full + wordWidth;
    cur.full += wordWidth + spaceWidth;
    cur.end = spaceEnd;
    i = spaceEnd;
  }
  if (cur.end != cur.first) lines.push_back(cur);
  return lines;
}

}  // namespace

// A node with inlineSize wraps its own text into a block `inlineSize` wide.
// The block starts at the first character's position, the first baseline sits
// on y, and text-anchor aligns each line inside the box, as upstream does.
QPointF placeWrapped(State& s, int nodeIndex, int first, int end, QPointF cursor) {
  const FlatNode& f = s.nodes[size_t(nodeIndex)];
  const Node& node = *f.node;
  Unit& head = s.units[size_t(first)];
  const double x0 = (unspecified(head.x) ? cursor.x() : head.x) + head.dx;
  const double y0 = (unspecified(head.y) ? cursor.y() : head.y) + head.dy;
  const double width = *node.inlineSize;
  const FontMetrics& m = s.probes.get(node.font, node.font.ligatures).metrics;
  const double lineHeight = (m.ascent + m.descent + m.leading) * node.font.size;

  const std::vector<Line> lines = breakLines(s, first, end, width);
  QPointF out = cursor;
  for (size_t k = 0; k < lines.size(); ++k) {
    const Line& line = lines[k];
    double shift = 0;
    if (node.anchor == Anchor::Middle) shift = (width - line.width) / 2;
    else if (node.anchor == Anchor::End) shift = width - line.width;
    double run = x0 + shift;
    const double baseline = y0 + double(k) * lineHeight;
    for (int i = line.first; i < line.end; ++i) {
      Unit& u = s.units[size_t(i)];
      u.pos = run + u.kern;
      u.perp = baseline + u.baselineOffset;
      u.end = u.pos + u.advance + u.spacing;
      u.chunk = -1;
      run = u.end;
    }
    out = QPointF(run, baseline);
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::text::detail
