#include "FilterRegion.h"

#include <algorithm>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// Coordinates past this are treated as not representable. It leaves room for
// the region cap to do the real limiting while keeping every later int
// conversion in range.
constexpr double kCoordLimit = 1.0e8;

double viewportExtent(const QSizeF& viewport, Axis axis) {
  switch (axis) {
    case Axis::X: return viewport.width();
    case Axis::Y: return viewport.height();
    case Axis::Diagonal: return std::sqrt((viewport.width() * viewport.width() + viewport.height() * viewport.height()) / 2.0);
  }
  return 0;
}

double bboxExtent(const QRectF& bbox, Axis axis) {
  switch (axis) {
    case Axis::X: return bbox.width();
    case Axis::Y: return bbox.height();
    case Axis::Diagonal: return std::sqrt((bbox.width() * bbox.width() + bbox.height() * bbox.height()) / 2.0);
  }
  return 0;
}

// A coordinate or size from a Length, in user space. `origin` adds the
// bounding box origin for objectBoundingBox x/y.
double resolve(const Length& l, Axis axis, bool obb, bool origin, const Env& env) {
  const Context& c = *env.context;
  double v = l.value;
  if (obb) {
    if (l.unit == Length::Unit::Percent) v /= 100.0;
    v *= bboxExtent(env.bbox, axis);
    if (origin) v += axis == Axis::X ? env.bbox.x() : env.bbox.y();
    return v;
  }
  switch (l.unit) {
    case Length::Unit::Px: return v;
    case Length::Unit::Percent: return v / 100.0 * viewportExtent(c.viewport, axis);
    case Length::Unit::Em: return v * c.fontSize;
  }
  return v;
}

bool finite(double v) { return std::isfinite(v) && std::abs(v) < kCoordLimit; }

}  // namespace

double Env::userNumber(double v, Axis axis) const { return primitiveObb ? v * bboxExtent(bbox, axis) : v; }

double Env::userLength(const Length& l, Axis axis) const {
  if (!l.set) return 0;
  return resolve(l, axis, primitiveObb, false, *this);
}

QPointF Env::vectorToDevice(double dx, double dy) const {
  return {toDevice.m11() * dx + toDevice.m21() * dy, toDevice.m12() * dx + toDevice.m22() * dy};
}

QPointF Env::extentToDevice(double ux, double uy) const {
  const double a = toDevice.m11() * ux, b = toDevice.m21() * uy;
  const double c = toDevice.m12() * ux, d = toDevice.m22() * uy;
  return {std::sqrt(a * a + b * b), std::sqrt(c * c + d * d)};
}

QRect toDeviceRect(const QRectF& user, const QTransform& toDevice) {
  if (!finite(user.x()) || !finite(user.y()) || !finite(user.width()) || !finite(user.height())) return {};
  const QRectF d = toDevice.mapRect(user);
  if (!finite(d.left()) || !finite(d.top()) || !finite(d.width()) || !finite(d.height())) return {};
  // The origin is floored and the size rounded up, the way resvg snaps a
  // region to pixels. Rounding the far edge out instead would add a column and
  // a row whenever the origin has a fraction.
  const int l = static_cast<int>(std::floor(d.left()));
  const int t = static_cast<int>(std::floor(d.top()));
  const int w = static_cast<int>(std::ceil(d.width()));
  const int h = static_cast<int>(std::ceil(d.height()));
  if (w <= 0 || h <= 0) return {};
  return QRect(l, t, w, h);
}

std::optional<QRectF> resolveSubregion(const Subregion& sub, const Env& env) {
  const bool obb = env.primitiveObb;
  const QRectF& f = env.userRegion;
  const double x = sub.x.set ? resolve(sub.x, Axis::X, obb, true, env) : f.x();
  const double y = sub.y.set ? resolve(sub.y, Axis::Y, obb, true, env) : f.y();
  const double w = sub.width.set ? resolve(sub.width, Axis::X, obb, false, env) : f.width();
  const double h = sub.height.set ? resolve(sub.height, Axis::Y, obb, false, env) : f.height();
  if (!finite(x) || !finite(y) || !finite(w) || !finite(h)) return std::nullopt;
  return QRectF(x, y, w, h);
}

std::optional<Env> makeEnv(const Definition& definition, const Context& context) {
  Env env;
  env.context = &context;
  env.toDevice = context.userToDevice;
  bool ok = false;
  env.fromDevice = env.toDevice.inverted(&ok);
  if (!ok || !std::isfinite(env.toDevice.determinant()) || env.toDevice.determinant() == 0) return std::nullopt;
  env.bbox = context.objectBoundingBox;
  env.primitiveObb = !definition.region.primitiveUserSpace;
  env.scaleX = std::hypot(env.toDevice.m11(), env.toDevice.m12());
  env.scaleY = std::hypot(env.toDevice.m21(), env.toDevice.m22());

  const FilterRegion& r = definition.region;
  const bool obb = !r.filterUserSpace;
  // Either unit system that refers to the bounding box needs a box with area.
  const bool boxUsable = env.bbox.isValid() && env.bbox.width() > 0 && env.bbox.height() > 0;
  if ((obb || env.primitiveObb) && !boxUsable) return std::nullopt;
  auto orDefault = [](const Length& l, double percent) {
    return l.set ? l : Length{percent, Length::Unit::Percent, true};
  };
  const double x = resolve(orDefault(r.x, -10), Axis::X, obb, true, env);
  const double y = resolve(orDefault(r.y, -10), Axis::Y, obb, true, env);
  const double w = resolve(orDefault(r.width, 120), Axis::X, obb, false, env);
  const double h = resolve(orDefault(r.height, 120), Axis::Y, obb, false, env);
  if (!finite(x) || !finite(y) || !finite(w) || !finite(h) || w <= 0 || h <= 0) return std::nullopt;
  env.userRegion = QRectF(x, y, w, h);
  env.region = toDeviceRect(env.userRegion, env.toDevice);
  if (env.region.isEmpty()) return std::nullopt;
  if (static_cast<qint64>(env.region.width()) * env.region.height() > kMaxRegionPixels) {
    if (context.workArea.isNull()) return std::nullopt;
    env.region = env.region.intersected(context.workArea);
    if (env.region.isEmpty() || static_cast<qint64>(env.region.width()) * env.region.height() > kMaxRegionPixels) {
      return std::nullopt;
    }
  }
  return env;
}

QImage makeImage(const QSize& size) {
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  if (!image.isNull()) image.fill(Qt::transparent);
  return image;
}

void clipToRect(QImage& image, const QRect& keep) {
  if (image.isNull()) return;
  const QRect k = keep.intersected(image.rect());
  const int bpl = image.bytesPerLine();
  for (int y = 0; y < image.height(); ++y) {
    uchar* line = image.scanLine(y);
    if (y < k.top() || y > k.bottom() || k.isEmpty()) {
      std::fill(line, line + bpl, uchar{0});
      continue;
    }
    std::fill(line, line + k.left() * 4, uchar{0});
    std::fill(line + (k.right() + 1) * 4, line + image.width() * 4, uchar{0});
  }
}

}  // namespace rnqnx::views::rnsvg::filter::detail
