#pragma once

#include "Style.h"

#include <QPainterPath>
#include <QString>

namespace rnqnx::views {
class RnsvgElementItem;
}

namespace rnqnx::views::rnsvg {

class Scene;

// What `pointerEvents` lets an element receive. The package's TypeScript type
// is React Native's (auto, none, box-none, box-only); SVG's own values
// (visiblePainted, visibleFill, visibleStroke, visible, painted, fill, stroke,
// all) are honored as well.
struct PointerMode {
  enum class Region {
    None,      // the element itself is never a target
    Painted,   // fill where there is a fill paint, stroke where there is a stroke paint
    Fill,      // the fill area whatever the paint
    Stroke,    // the stroke area whatever the paint
    All,       // fill and stroke areas whatever the paint
    Box,       // the element's bounding box
  };
  Region region{Region::Painted};
  bool descend{true};  // children can be targets (false for none and box-only)

  static PointerMode parse(const QString& value);
};

// Decides whether the point being tested is on an element's geometry.
class HitTester {
 public:
  explicit HitTester(Scene& scene) : scene_(scene) {}

  // Tests the hit point against `path` (in the element's own space, `f.ctm`
  // maps it to item space) and records `e` or its nearest responsible
  // ancestor as the target if the point is on it.
  void testPath(const RnsvgElementItem* e, const QPainterPath& path, const Frame& f, const PointerMode& mode);

 private:
  Scene& scene_;
};

}  // namespace rnqnx::views::rnsvg
