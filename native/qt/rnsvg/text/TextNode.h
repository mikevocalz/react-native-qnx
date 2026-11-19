#pragma once

// Input of the SVG text layout engine: a tree of already-resolved text nodes.
//
// Everything here is a value type. The scene builds the tree from
// ElementProps (inheritance applied, lengths converted to user-space doubles),
// so the engine never touches QQuickItem, the element items or Fabric.

#include <QPainterPath>
#include <QString>
#include <optional>
#include <variant>
#include <vector>

namespace rnqnx::views::rnsvg::text {

enum class Anchor { Start, Middle, End };
enum class LengthAdjust { Spacing, SpacingAndGlyphs };

// SVG 1.1 / CSS inline alignment-baseline values. Auto, alphabetic and
// baseline all place the glyph baseline on the text position.
enum class AlignmentBaseline {
  Baseline,
  Alphabetic,
  Ideographic,
  Middle,
  Central,
  Mathematical,
  Hanging,
  TextTop,     // text-top, text-before-edge, before-edge
  TextBottom,  // text-bottom, text-after-edge, after-edge
  Top,
  Center,
  Bottom,
};

enum class ShiftKeyword { Baseline, Sub, Super };
// Unset, a distance in user units (positive raises the text), or a keyword.
using BaselineShift = std::variant<std::monostate, double, ShiftKeyword>;

enum Decoration : unsigned {
  DecorationNone = 0,
  DecorationUnderline = 1,
  DecorationOverline = 2,
  DecorationLineThrough = 4,
};

enum class PathSide { Left, Right };
enum class PathMethod { Align, Stretch };
enum class PathSpacing { Auto, Exact };
enum class PathMidLine { Smooth, Sharp };

struct FontFeature {
  QString tag;  // four letters, e.g. "liga"
  unsigned value{1};
};

// A font after inheritance. Sizes are user units.
struct Font {
  QString family;
  double size{12};
  int weight{400};  // 100..900
  bool italic{false};
  double stretchPercent{100};  // font-stretch keyword mapped by parseStretch
  double letterSpacing{0};
  double wordSpacing{0};
  std::optional<double> kerning;  // nullopt: font kerning; a number replaces it
  bool ligatures{true};           // font-variant-ligatures normal / none
  std::vector<FontFeature> features;
  unsigned decoration{DecorationNone};
};

struct PathOffset {
  double value{0};
  bool percent{false};  // of the path length
};

// A textPath element's own props. The path is in the text element's user
// space; the engine lays the first subpath out.
struct TextPathSpec {
  QPainterPath path;
  PathOffset startOffset;
  PathSide side{PathSide::Left};
  PathMethod method{PathMethod::Align};
  PathSpacing spacing{PathSpacing::Auto};
  PathMidLine midLine{PathMidLine::Smooth};
};

// One <text>, <tspan> or <textPath>. `x`, `y`, `dx`, `dy` and `rotate` are
// per-character lists: entry i applies to the i-th UTF-16 unit of this node's
// subtree in document order. An absent (nullopt or NaN) entry leaves that
// character unchanged; `rotate` repeats its last value.
struct Node {
  int id{0};  // chosen by the caller, echoed in the result
  Font font;
  Anchor anchor{Anchor::Start};
  QString content;  // this node's own characters
  std::vector<std::optional<double>> x, y, dx, dy;
  std::vector<double> rotate;
  std::optional<double> textLength;
  LengthAdjust lengthAdjust{LengthAdjust::Spacing};
  BaselineShift baselineShift;
  std::optional<AlignmentBaseline> alignmentBaseline;  // nullopt: inherit
  // The vertical-align shorthand as react-native-svg's Android view reads it:
  // "<baseline-shift> <alignment-baseline>", or one of the two alone. It only
  // fills the two fields above when they are unset.
  QString verticalAlign;
  std::optional<double> inlineSize;  // wraps this node's own content
  std::optional<TextPathSpec> textPath;
  std::vector<Node> children;
};

}  // namespace rnqnx::views::rnsvg::text
