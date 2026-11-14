#include "LayoutState.h"

#include <algorithm>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

void applyList(const std::vector<std::optional<double>>& list, const FlatNode& f, std::vector<double>& out) {
  const int count = std::min<int>(int(list.size()), f.endChar - f.firstChar);
  for (int i = 0; i < count; ++i) {
    if (list[i] && std::isfinite(*list[i])) out[f.firstChar + i] = *list[i];
  }
}

}  // namespace

// Entry i of a node's list belongs to the i-th character of its subtree. A
// deeper node's entry wins over an ancestor's for the same character, which
// is what the SVG 1.1 and SVG 2 resolve step gives (parents first, children
// after). Characters a list does not reach keep the ancestor's value.
void resolveCharAttrs(State& s) {
  const size_t n = size_t(s.totalChars);
  s.attrs.x.assign(n, kNaN);
  s.attrs.y.assign(n, kNaN);
  s.attrs.dx.assign(n, kNaN);
  s.attrs.dy.assign(n, kNaN);
  s.attrs.rotate.assign(n, kNaN);
  for (const FlatNode& f : s.nodes) {
    const Node& node = *f.node;
    applyList(node.x, f, s.attrs.x);
    applyList(node.y, f, s.attrs.y);
    applyList(node.dx, f, s.attrs.dx);
    applyList(node.dy, f, s.attrs.dy);
    if (!node.rotate.empty()) {
      // The last rotate value repeats for the rest of this node's characters.
      for (int c = f.firstChar; c < f.endChar; ++c) {
        const size_t i = std::min(size_t(c - f.firstChar), node.rotate.size() - 1);
        const double r = node.rotate[i];
        s.attrs.rotate[size_t(c)] = std::isfinite(r) ? r : 0;
      }
    }
  }
}

}  // namespace rnqnx::views::rnsvg::text::detail
