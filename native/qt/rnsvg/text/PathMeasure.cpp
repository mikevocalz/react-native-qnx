#include "PathMeasure.h"

#include "TextLayout.h"

#include <QPolygonF>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::text {

namespace {

// Curves are flattened at this multiple of user space, so the polyline stays
// within a small fraction of a unit of the true curve.
constexpr double kFlatten = 8;

double distanceBetween(QPointF a, QPointF b) { return std::hypot(a.x() - b.x(), a.y() - b.y()); }

bool finite(const QPointF& p) { return std::isfinite(p.x()) && std::isfinite(p.y()); }

}  // namespace

PathMeasure::PathMeasure(const QPainterPath& path, bool reverse, bool& truncated) {
  if (path.isEmpty()) return;
  const QList<QPolygonF> polygons = path.toSubpathPolygons(QTransform::fromScale(kFlatten, kFlatten));
  const QPolygonF* chosen = nullptr;
  for (const QPolygonF& polygon : polygons) {
    if (polygon.size() >= 2) {
      chosen = &polygon;
      break;
    }
  }
  if (!chosen) return;

  std::vector<QPointF> raw;
  raw.reserve(size_t(std::min<qsizetype>(chosen->size(), kMaxPathSegments + 1)));
  for (const QPointF& p : *chosen) {
    if (int(raw.size()) > kMaxPathSegments) {
      truncated = true;
      break;
    }
    const QPointF q(p.x() / kFlatten, p.y() / kFlatten);
    if (!finite(q)) return;
    raw.push_back(q);
  }
  if (raw.size() < 2) return;
  closed_ = distanceBetween(raw.front(), raw.back()) < 1e-9 && raw.size() > 2 && !truncated;
  if (reverse) std::reverse(raw.begin(), raw.end());

  points_.push_back(raw.front());
  cumulative_.push_back(0);
  for (size_t i = 1; i < raw.size(); ++i) {
    const double d = distanceBetween(points_.back(), raw[i]);
    if (d < 1e-12) continue;
    points_.push_back(raw[i]);
    cumulative_.push_back(cumulative_.back() + d);
  }
  length_ = points_.size() >= 2 ? cumulative_.back() : 0;
  if (length_ <= 0) closed_ = false;
}

PathMeasure::Sample PathMeasure::at(double distance) const {
  if (!valid() || !std::isfinite(distance)) return {QPointF(), QPointF(1, 0)};
  const auto direction = [&](size_t seg) {
    const QPointF d = points_[seg + 1] - points_[seg];
    const double len = cumulative_[seg + 1] - cumulative_[seg];
    return QPointF(d.x() / len, d.y() / len);
  };
  if (closed_) {
    distance = std::fmod(distance, length_);
    if (distance < 0) distance += length_;
  } else if (distance < 0) {
    const QPointF t = direction(0);
    return {points_.front() + t * distance, t};
  } else if (distance > length_) {
    const QPointF t = direction(points_.size() - 2);
    return {points_.back() + t * (distance - length_), t};
  }
  const auto it = std::upper_bound(cumulative_.begin(), cumulative_.end(), distance);
  size_t seg = size_t(std::max<std::ptrdiff_t>(it - cumulative_.begin() - 1, 0));
  seg = std::min(seg, points_.size() - 2);
  const QPointF t = direction(seg);
  return {points_[seg] + t * (distance - cumulative_[seg]), t};
}

}  // namespace rnqnx::views::rnsvg::text
