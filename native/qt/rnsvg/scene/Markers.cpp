#include "Markers.h"

#include "Geometry.h"
#include "Scene.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace rnqnx::views::rnsvg {
namespace {

constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
constexpr size_t kMaxMarkedVertices = 4096;

enum class Position { Start, Mid, End };

double angleOf(const QPointF& d) { return std::atan2(d.y(), d.x()) * kRadiansToDegrees; }

bool isZero(const QPointF& d) { return d.x() == 0 && d.y() == 0; }

// The direction a marker with orient="auto" points at `v`: along the path at
// the ends, along the bisector of the two segments in between.
double autoAngle(const PathVertex& v) {
  const bool hasIn = !isZero(v.inDirection), hasOut = !isZero(v.outDirection);
  if (hasIn && hasOut) {
    const QPointF sum = v.inDirection + v.outDirection;
    return isZero(sum) ? angleOf(v.outDirection) : angleOf(sum);
  }
  if (hasOut) return angleOf(v.outDirection);
  if (hasIn) return angleOf(v.inDirection);
  return 0;
}

// The marker's placement in the element's user space, or false when the marker
// has no area.
struct Placement {
  QTransform toUser;      // marker viewport space to element user space
  QTransform content;     // marker content space to element user space (viewBox applied)
  QRectF viewport;        // the clip, in marker viewport space
  QSizeF contentViewport; // for percentage lengths inside
};

bool place(const ElementProps& marker, const Frame& f, double strokeWidth, const PathVertex& v, Position position,
           Placement& out) {
  const Units u(f);
  const double mw = u.resolve(marker.markerWidth, Axis::X, 3);
  const double mh = u.resolve(marker.markerHeight, Axis::Y, 3);
  if (!(mw > 0) || !(mh > 0)) return false;
  double angle = 0;
  if (marker.orient == QStringLiteral("auto")) {
    angle = autoAngle(v);
  } else if (marker.orient == QStringLiteral("auto-start-reverse")) {
    angle = autoAngle(v) + (position == Position::Start ? 180.0 : 0.0);
  } else if (!marker.orient.isEmpty()) {
    bool ok = false;
    const double a = QString(marker.orient).remove(QStringLiteral("deg")).toDouble(&ok);
    if (ok) angle = a;
  }
  const double scale = marker.markerStrokeWidthUnits ? strokeWidth : 1.0;
  if (!(scale > 0)) return false;
  const QTransform viewBox = viewBoxTransform(marker.viewBox, QSizeF(mw, mh));
  const QPointF ref = viewBox.map(QPointF(u.resolve(marker.refX, Axis::X), u.resolve(marker.refY, Axis::Y)));
  QTransform t;
  t.translate(v.point.x(), v.point.y());
  t.rotate(angle);
  t.scale(scale, scale);
  t.translate(-ref.x(), -ref.y());
  out.toUser = t;
  out.content = viewBox * t;
  out.viewport = QRectF(0, 0, mw, mh);
  out.contentViewport = marker.viewBox.set ? QSizeF(marker.viewBox.width, marker.viewBox.height) : QSizeF(mw, mh);
  return true;
}

const RnsvgElementItem* markerFor(const Scene& scene, const QString& id) {
  if (id.isEmpty()) return nullptr;
  const auto* e = scene.byId(id);
  return e && e->props().kind == Kind::Marker ? e : nullptr;
}

}  // namespace

void Markers::draw(const RnsvgElementItem& e, const Frame& f, const QRectF& bbox, int depth) {
  const auto& st = f.style;
  const auto* start = markerFor(scene_, st.markerStart);
  const auto* mid = markerFor(scene_, st.markerMid);
  const auto* end = markerFor(scene_, st.markerEnd);
  if (!start && !mid && !end) return;
  const Units units(f);
  const std::vector<PathVertex> vertices = shapeVertices(e.props(), units);
  if (vertices.empty() || vertices.size() > kMaxMarkedVertices) return;
  const double strokeWidth = std::max(0.0, units.resolve(st.strokeWidth, Axis::Diag, 1));

  scene_.pushContext({st.fill, st.stroke, bbox, f});
  for (size_t i = 0; i < vertices.size(); ++i) {
    const Position position = i == 0 ? Position::Start : i + 1 == vertices.size() ? Position::End : Position::Mid;
    const auto* marker = position == Position::Start ? start : position == Position::End ? end : mid;
    if (!marker) continue;
    const auto& mp = marker->props();
    Placement placement;
    if (!place(mp, f, strokeWidth, vertices[i], position, placement)) continue;

    Frame mf;
    mf.style = Style();
    mf.style.color = f.style.color;
    applyProps(mf.style, mp);
    mf.ctm = placement.content * f.ctm;
    mf.viewport = placement.contentViewport;
    // No clip to the marker viewport: react-native-svg's native marker does not
    // clip either, and the package has no `overflow` prop to ask for it.
    scene_.renderChildren(marker, mf, depth + 1);
  }
  scene_.popContext();
}

QRectF Markers::bounds(const RnsvgElementItem* e, const Frame& f, int depth) {
    const auto& st = f.style;
  const auto* start = markerFor(scene_, st.markerStart);
  const auto* mid = markerFor(scene_, st.markerMid);
  const auto* end = markerFor(scene_, st.markerEnd);
  if (!start && !mid && !end) return {};
  const Units units(f);
  const std::vector<PathVertex> vertices = shapeVertices(e->props(), units);
  if (vertices.empty() || vertices.size() > kMaxMarkedVertices) return {};
  const double strokeWidth = std::max(0.0, units.resolve(st.strokeWidth, Axis::Diag, 1));
  // What each marker paints, in its own content space.
  auto contentBox = [&](const RnsvgElementItem* marker, const Placement& placement) {
    Frame mf;
    mf.style = Style();
    mf.style.color = f.style.color;
    applyProps(mf.style, marker->props());
    mf.viewport = placement.contentViewport;
    QRectF box;
    for (const auto* c : marker->elementChildren()) {
      QRectF cb = scene_.bounds().paint(c, mf, depth + 1);
      if (!cb.isNull() && c->props().matrix) cb = c->props().matrix->mapRect(cb);
      if (!cb.isNull()) box = box.isNull() ? cb : box.united(cb);
    }
    return box;
  };
  // The content of a marker does not depend on the vertex: measure it once.
  std::unordered_map<const RnsvgElementItem*, QRectF> contents;
  QRectF box;
  for (size_t i = 0; i < vertices.size(); ++i) {
    const Position position = i == 0 ? Position::Start : i + 1 == vertices.size() ? Position::End : Position::Mid;
    const auto* marker = position == Position::Start ? start : position == Position::End ? end : mid;
    if (!marker) continue;
    Placement placement;
    if (!place(marker->props(), f, strokeWidth, vertices[i], position, placement)) continue;
    auto found = contents.find(marker);
    if (found == contents.end()) found = contents.emplace(marker, contentBox(marker, placement)).first;
    const QRectF content = found->second;
    if (content.isNull()) continue;
    const QRectF r = placement.content.mapRect(content);
    box = box.isNull() ? r : box.united(r);
  }
  return box;
}

}  // namespace rnqnx::views::rnsvg
