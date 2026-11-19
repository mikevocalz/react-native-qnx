#include "LayoutState.h"

#include <algorithm>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

constexpr int kMaxNodes = 20000;

// SVG collapses white space before layout; whatever survives to here is laid
// out as a plain space so that Qt never splits the node into several lines.
QString cleaned(const QString& in) {
  QString out = in;
  for (QChar& c : out) {
    if (c.unicode() < 0x20 || c == QChar(0x2028) || c == QChar(0x2029) || c == QChar(0x85)) c = QLatin1Char(' ');
  }
  return out;
}

// Past this size coordinates lose meaning and outline arithmetic can overflow.
constexpr double kMaxFontSize = 1e7;

bool validSize(double size) { return std::isfinite(size) && size > 0 && size <= kMaxFontSize; }

class Flattener {
 public:
  explicit Flattener(State& s) : s_(s) {}

  void visit(const Node& node, int parent, int depth) {
    if (depth > kMaxDepth || int(s_.nodes.size()) >= kMaxNodes) {
      s_.truncated = true;
      return;
    }
    const int index = int(s_.nodes.size());
    s_.nodes.emplace_back();
    {
      FlatNode& f = s_.nodes[index];
      f.node = &node;
      f.parent = parent;
      f.valid = validSize(node.font.size);
      QString content = f.valid ? cleaned(node.content) : QString();
      const int budget = std::max(0, kMaxGlyphs - s_.totalChars);
      if (content.size() > budget) {
        content.truncate(budget);
        s_.truncated = true;
      }
      f.content = content;
      f.firstChar = f.ownFirstChar = s_.totalChars;
      s_.totalChars += int(content.size());

      std::optional<AlignmentBaseline> baseline;
      ownBaselineProps(node, baseline, f.shift);
      const FlatNode* up = parent >= 0 ? &s_.nodes[parent] : nullptr;
      f.baseline = baseline.value_or(up ? up->baseline : AlignmentBaseline::Baseline);
      if (std::holds_alternative<std::monostate>(f.shift) && up) f.shift = up->shift;
      f.pathNode = node.textPath ? index : (up ? up->pathNode : -1);
      const bool hasLength = node.textLength && std::isfinite(*node.textLength) && *node.textLength >= 0;
      f.lengthNode = hasLength ? index : (up ? up->lengthNode : -1);
    }
    for (const Node& child : node.children) visit(child, index, depth + 1);
    FlatNode& f = s_.nodes[index];
    f.endChar = s_.totalChars;
    f.subtreeEnd = int(s_.nodes.size());
  }

 private:
  State& s_;
};

}  // namespace

void flatten(const Node& root, State& s) { Flattener(s).visit(root, -1, 0); }

}  // namespace rnqnx::views::rnsvg::text::detail
