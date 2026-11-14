#pragma once

// Arc-length parameterisation of a QPainterPath's first subpath, flattened to
// a polyline. Positions beyond either end extrapolate along the end tangents
// (open paths) or wrap around (closed paths).

#include <QPainterPath>
#include <QPointF>
#include <vector>

namespace rnqnx::views::rnsvg::text {

class PathMeasure {
 public:
  struct Sample {
    QPointF point;
    QPointF tangent;  // unit vector in the direction of travel
  };

  // `reverse` measures the path backwards (textPath side="right"). Sets
  // `truncated` when the path held more segments than kMaxPathSegments.
  PathMeasure(const QPainterPath& path, bool reverse, bool& truncated);

  bool valid() const { return length_ > 0; }
  double length() const { return length_; }
  bool closed() const { return closed_; }
  Sample at(double distance) const;

 private:
  std::vector<QPointF> points_;
  std::vector<double> cumulative_;  // cumulative_[i] is the length up to points_[i]
  double length_{0};
  bool closed_{false};
};

}  // namespace rnqnx::views::rnsvg::text
