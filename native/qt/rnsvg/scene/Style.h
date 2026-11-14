#pragma once

#include "../RnsvgModel.h"

#include <QSizeF>
#include <QTransform>
#include <vector>

namespace rnqnx::views::rnsvg {

// The paint and font state SVG inherits down the tree. Each element copies its
// parent's Style and overwrites the properties its own propList names.
struct Style {
  Brush fill{Brush::Type::Color, QColor(Qt::black), {}};
  Brush stroke;
  double fillOpacity{1}, strokeOpacity{1};
  bool fillEvenOdd{false};
  int clipRule{-1};  // -1 unset (nonzero), 0 evenodd, 1 nonzero
  Length strokeWidth{1, Length::Unit::Px, true};
  int cap{0}, join{0};
  std::vector<Length> dash;
  double dashOffset{0};
  double miter{4};
  bool nonScaling{false};
  QColor color{Qt::black};
  FontSpec font;
  QString markerStart, markerMid, markerEnd;
};

// A traversal frame: the user space of the element being drawn and what it
// inherited.
struct Frame {
  QTransform ctm;  // user space of the current element to root item space
  Style style;
  QSizeF viewport;  // nearest viewport, for percentage lengths
};

enum class Axis { X, Y, Diag };

// Merges the props `p` set (per its propList) over `s`.
void applyProps(Style& s, const ElementProps& p);

QColor withAlpha(QColor c, double a);

// Resolves lengths against one frame: percentages against the viewport,
// em against the font size.
class Units {
 public:
  Units(const QSizeF& viewport, double fontSize) : viewport_(viewport), fontSize_(fontSize) {}
  explicit Units(const Frame& f) : Units(f.viewport, f.style.font.size.value_or(12.0)) {}

  double resolve(const Length& l, Axis axis, double fallback = 0) const;
  // objectBoundingBox lengths are fractions (a percentage is a fraction too);
  // userSpaceOnUse lengths resolve as usual.
  double resolveUnits(const Length& l, bool userSpace, Axis axis, double fallback) const;
  const QSizeF& viewport() const { return viewport_; }
  double fontSize() const { return fontSize_; }

 private:
  QSizeF viewport_;
  double fontSize_;
};

}  // namespace rnqnx::views::rnsvg
