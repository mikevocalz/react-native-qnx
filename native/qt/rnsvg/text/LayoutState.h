#pragma once

// Internal state shared by the layout passes. Not part of the public
// interface: callers use TextLayout.h.

#include "FontProbe.h"
#include "TextLayout.h"

#include <cmath>
#include <limits>
#include <vector>

namespace rnqnx::views::rnsvg::text::detail {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

inline bool unspecified(double v) { return std::isnan(v); }

// A node after flattening, in document (pre) order.
struct FlatNode {
  const Node* node{nullptr};
  int parent{-1};
  int subtreeEnd{0};      // flat index one past the node's subtree
  int firstChar{0};       // subtree character range [firstChar, endChar)
  int endChar{0};
  int ownFirstChar{0};    // where this node's own content starts
  QString content;        // own content after capping and control-character cleanup
  int firstUnit{0};       // subtree unit range [firstUnit, endUnit)
  int endUnit{0};
  int ownFirstUnit{0};    // own units [ownFirstUnit, ownEndUnit)
  int ownEndUnit{0};
  int pathNode{-1};       // nearest ancestor-or-self with a textPath
  int lengthNode{-1};     // nearest ancestor-or-self with a valid textLength
  AlignmentBaseline baseline{AlignmentBaseline::Baseline};
  BaselineShift shift;
  bool valid{true};       // false: the font size cannot be laid out
};

// Per-character resolution of x, y, dx, dy and rotate over the whole text.
// NaN marks an entry nobody specified.
struct CharAttrs {
  std::vector<double> x, y, dx, dy, rotate;
};

struct Unit {
  int node{0};
  int charIndex{0};
  int charCount{1};
  double size{12};
  double advance{0};  // user units, before textLength scaling
  QPainterPath outline;  // em units
  bool space{false};
  bool wordSeparator{false};
  double spacing{0};     // letter-spacing plus word-spacing after this glyph
  double kern{0};        // explicit kerning added before this glyph
  double baselineOffset{0};  // alignment-baseline and baseline-shift, positive down
  double x{kNaN}, y{kNaN}, dx{0}, dy{0}, rotate{0};
  bool fixed{false};     // placed by inline-size wrapping, outside chunk anchoring
  int pathNode{-1};
  // Set by the textLength pass.
  double scaleX{1};
  double extra{0};
  // Set by placement.
  bool chunkStart{false};
  int chunk{-1};
  double pos{0};   // origin x, or distance along the path
  double perp{0};  // origin y, or offset from the path (baseline offset included)
  double end{0};   // pos plus the advance, spacing and textLength extra
};

struct Chunk {
  int first{0};
  int end{0};
  double startPos{0};
  double endPos{0};
  Anchor anchor{Anchor::Start};
};

struct State {
  std::vector<FlatNode> nodes;
  std::vector<Unit> units;
  std::vector<Chunk> chunks;
  CharAttrs attrs;
  int totalChars{0};
  bool truncated{false};
  ProbeCache probes;
  QPointF start;
};

// FlatTree.cpp
void flatten(const Node& root, State& s);
// CharAttrs.cpp
void resolveCharAttrs(State& s);
// UnitBuilder.cpp
void buildUnits(State& s);
// Baseline.cpp
void ownBaselineProps(const Node& node, std::optional<AlignmentBaseline>& baseline, BaselineShift& shift);
double baselineYOffset(AlignmentBaseline baseline, const BaselineShift& shift, const FontMetrics& m, double size);
// TextLength.cpp
void markChunkStarts(State& s);
void applyTextLength(State& s);
// Placement.cpp
void placeUnits(State& s);
void anchorChunks(State& s);
// InlineSize.cpp: places the own units [first, end) of a wrapped node; returns the new cursor.
QPointF placeWrapped(State& s, int node, int first, int end, QPointF cursor);
// GlyphEmit.cpp
void emitGeometry(State& s, Result& out);

}  // namespace rnqnx::views::rnsvg::text::detail
