#pragma once

// Value types shared by the element model and the filter model. Kept free of
// anything else in this directory so each of them can include it.

#include <QColor>
#include <QPointF>
#include <QString>

namespace rnqnx::views::rnsvg {

// A react-native-svg NumberProp: a number, or a string with a unit.
struct Length {
  enum class Unit { Px, Percent, Em };
  double value{0};
  Unit unit{Unit::Px};
  bool set{false};
};

// react-native-svg extractBrush(): type 0 color, 1 url(#ref), 2 currentColor,
// 3 context-fill, 4 context-stroke; `none` is a null prop.
struct Brush {
  enum class Type { None, Color, Ref, CurrentColor, ContextFill, ContextStroke };
  Type type{Type::None};
  QColor color;
  QString ref;
};

// One vertex of a path as its data wrote it (an arc is one vertex pair, not
// the several cubics Qt stores). Directions are unit vectors in path space; a
// zero vector means the direction is undefined there (a zero-length segment).
struct PathVertex {
  QPointF point;
  QPointF inDirection;   // direction of travel arriving at the vertex
  QPointF outDirection;  // direction of travel leaving it
  bool subpathStart{false};
  bool subpathEnd{false};
  bool closesSubpath{false};  // `z` arrived at this vertex
};

}  // namespace rnqnx::views::rnsvg
