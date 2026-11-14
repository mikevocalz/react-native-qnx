#pragma once

// Builds the QFont the engine shapes with and reads the metrics the layout
// needs. Glyph outlines are made at a large probe pixel size with hinting off
// and scaled down, so advances stay linear in font size.

#include "TextNode.h"

#include <QFont>
#include <map>
#include <memory>
#include <tuple>

namespace rnqnx::views::rnsvg::text {

constexpr double kProbeSize = 100;

// All values are fractions of the font size (em), positive up unless noted.
struct FontMetrics {
  double ascent{0.8};
  double descent{0.2};  // positive down
  double leading{0};
  double xHeight{0.5};
  // Decoration lines: offsets are the distance from the baseline to the
  // centre of the stroke (post.underlinePosition and OS/2.yStrikeoutPosition,
  // read the way FreeType and resvg read them); one thickness serves all lines.
  double underlinePos{0.05};  // positive down
  double strikePos{0.3};      // positive up
  double lineThickness{0.05};
  double subscriptDrop{0.2};     // OS/2 ySubscriptYOffset: how far `sub` lowers
  double superscriptRise{0.4};   // OS/2 ySuperscriptYOffset: how far `super` raises
};

struct Probe {
  QFont font;               // pixel size kProbeSize, no hinting, no letter spacing
  FontMetrics metrics;
  double stretchFix{1};     // extra horizontal scale when Qt did not apply font-stretch
  double italicShear{0};    // x shear per unit of height when no italic face and Qt left the outlines upright
  double emboldenPx{0};     // probe pixels to thicken outlines by when the family has no bold face
};

// One layout call keeps one cache, so a tree that repeats a font builds it once.
class ProbeCache {
 public:
  const Probe& get(const Font& font, bool ligatures);

 private:
  using Key = std::tuple<QString, int, bool, int, bool, bool, QString>;
  std::map<Key, std::unique_ptr<Probe>> probes_;
};

}  // namespace rnqnx::views::rnsvg::text
