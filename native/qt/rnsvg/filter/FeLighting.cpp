#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

struct Vec3 {
  double x{0}, y{0}, z{0};
};

Vec3 normalized(Vec3 v) {
  const double n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  if (n == 0 || !std::isfinite(n)) return {0, 0, 1};
  return {v.x / n, v.y / n, v.z / n};
}

double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double finiteOr0(double v) { return std::isfinite(v) ? v : 0.0; }

// Surface normal from the alpha plane, following the Sobel-style kernels of
// the specification. The edge and corner kernels in the specification are the
// interior one restricted to the neighbours that exist: the difference is
// taken across the available columns (or rows), weighted 1-2-1 over the
// available rows (or columns), scaled by 2 / (spacing * weight sum).
Vec3 surfaceNormal(const std::vector<float>& alpha, int w, int h, int x, int y, double surfaceScale) {
  auto A = [&](int px, int py) { return static_cast<double>(alpha[static_cast<size_t>(py) * w + px]); };
  const int xm = x > 0 ? x - 1 : x, xp = x < w - 1 ? x + 1 : x;
  const int ym = y > 0 ? y - 1 : y, yp = y < h - 1 ? y + 1 : y;
  double nx = 0, ny = 0;
  if (xp != xm) {
    double sum = 0, weights = 0;
    for (int r = ym; r <= yp; ++r) {
      const double wt = r == y ? 2.0 : 1.0;
      sum += wt * (A(xp, r) - A(xm, r));
      weights += wt;
    }
    nx = -surfaceScale * (2.0 / (xp - xm)) / weights * sum;
  }
  if (yp != ym) {
    double sum = 0, weights = 0;
    for (int c = xm; c <= xp; ++c) {
      const double wt = c == x ? 2.0 : 1.0;
      sum += wt * (A(c, yp) - A(c, ym));
      weights += wt;
    }
    ny = -surfaceScale * (2.0 / (yp - ym)) / weights * sum;
  }
  return normalized({nx, ny, 1.0});
}

struct Light {
  LightSource::Kind kind;
  Vec3 direction;       // distant: toward the light
  Vec3 position;        // point, spot: device pixels relative to the region
  Vec3 axis;            // spot: unit vector from the light toward pointsAt
  double spotExponent{1};
  bool cone{false};
  double cosCone{0};
};

Light deviceLight(const Env& env, const LightSource& s) {
  Light l;
  l.kind = s.kind;
  if (s.kind == LightSource::Kind::Distant) {
    const double az = finiteOr0(s.azimuth) * kPi / 180.0, el = finiteOr0(s.elevation) * kPi / 180.0;
    l.direction = normalized({std::cos(az) * std::cos(el), std::sin(az) * std::cos(el), std::sin(el)});
    return l;
  }
  const double zScale = (env.scaleX + env.scaleY) / 2.0;
  auto toDevice = [&](double x, double y, double z) {
    double ux = finiteOr0(x), uy = finiteOr0(y);
    if (env.primitiveObb) {
      ux = env.bbox.x() + ux * env.bbox.width();
      uy = env.bbox.y() + uy * env.bbox.height();
    }
    const QPointF d = env.toDevice.map(QPointF(ux, uy));
    return Vec3{d.x() - env.region.x(), d.y() - env.region.y(), env.userNumber(finiteOr0(z), Axis::Diagonal) * zScale};
  };
  l.position = toDevice(s.x, s.y, s.z);
  if (s.kind == LightSource::Kind::Spot) {
    const Vec3 at = toDevice(s.pointsAtX, s.pointsAtY, s.pointsAtZ);
    l.axis = normalized({at.x - l.position.x, at.y - l.position.y, at.z - l.position.z});
    l.spotExponent = s.specularExponent > 0 && std::isfinite(s.specularExponent) ? s.specularExponent : 1.0;
    l.cone = s.hasLimitingCone;
    l.cosCone = std::cos(std::abs(finiteOr0(s.limitingConeAngle)) * kPi / 180.0);
  }
  return l;
}

struct Rgbf {
  double r, g, b;
};

// The light color at a surface point; black outside a spot's cone.
Rgbf lightColorAt(const Light& l, Rgbf color, Vec3 toLight) {
  if (l.kind != LightSource::Kind::Spot) return color;
  const double minusLS = -dot(toLight, l.axis);
  if (minusLS <= 0 || (l.cone && minusLS < l.cosCone)) return {0, 0, 0};
  const double k = std::pow(minusLS, l.spotExponent);
  return {color.r * k, color.g * k, color.b * k};
}

// Shared by both lighting primitives. `shade(N, L, lightColor) -> premultiplied
// pixel` supplies the only difference between them.
template <class Shade>
Layer lightingPass(const Env& env, ColorSpace cs, const QRect& sub, const Layer& in, const LightSource& source,
                   double surfaceScale, const QColor& color, Shade shade) {
  Layer out{makeImage(env.region.size()), {}, cs};
  if (!std::isfinite(surfaceScale)) return out;
  const int w = in.image.width(), h = in.image.height();
  std::vector<float> alpha(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(in.image.constScanLine(y));
    for (int x = 0; x < w; ++x) alpha[static_cast<size_t>(y) * w + x] = qAlpha(s[x]) / 255.0f;
  }
  const QRgb lc = premultipliedIn(color, 1.0, cs);  // opaque, so premultiplied == plain
  const Rgbf lightRgb{qRed(lc) / 255.0, qGreen(lc) / 255.0, qBlue(lc) / 255.0};
  const Light light = deviceLight(env, source);
  const QRect r = sub.intersected(out.image.rect());
  for (int y = r.top(); y <= r.bottom(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(out.image.scanLine(y));
    for (int x = r.left(); x <= r.right(); ++x) {
      const Vec3 n = surfaceNormal(alpha, w, h, x, y, surfaceScale);
      Vec3 toLight = light.direction;
      if (light.kind != LightSource::Kind::Distant) {
        const double z = surfaceScale * alpha[static_cast<size_t>(y) * w + x];
        toLight = normalized({light.position.x - x, light.position.y - y, light.position.z - z});
      }
      line[x] = shade(n, toLight, lightColorAt(light, lightRgb, toLight));
    }
  }
  return out;
}

QRgb pack(double r, double g, double b, double a) {
  auto byte = [](double v) { return clampByte(static_cast<int>(std::lround(clamp01(v) * 255.0))); };
  return qRgba(byte(r), byte(g), byte(b), byte(a));
}

}  // namespace

// feDiffuseLighting: alpha is 1 everywhere in the subregion. A negative
// diffuseConstant is an error and gives a transparent result. Normals come
// from the whole input image (so the border kernels apply at the filter
// region edge), in device pixels. The distant light direction and the spot
// cone are not rotated with the transform.
Layer applyLighting(const Env& env, const DiffuseLighting& p, ColorSpace cs, const QRect& sub, const Layer& in) {
  if (!std::isfinite(p.diffuseConstant) || p.diffuseConstant < 0) return {makeImage(env.region.size()), {}, cs};
  const double kd = p.diffuseConstant;
  return lightingPass(env, cs, sub, in, p.light, p.surfaceScale, brushColor(p.lightingColor, env, QColor(Qt::white)),
                      [kd](Vec3 n, Vec3 l, Rgbf c) {
                        const double f = kd * std::max(0.0, dot(n, l));
                        return pack(f * c.r, f * c.g, f * c.b, 1.0);
                      });
}

// feSpecularLighting: alpha is the largest color channel, and the color
// channels are stored as computed (they are already at most alpha), which is
// what resvg does. specularExponent outside 1..128 and a negative
// specularConstant are errors and give a transparent result.
Layer applyLighting(const Env& env, const SpecularLighting& p, ColorSpace cs, const QRect& sub, const Layer& in) {
  if (!std::isfinite(p.specularConstant) || p.specularConstant < 0 || !std::isfinite(p.specularExponent) ||
      p.specularExponent < 1 || p.specularExponent > 128) {
    return {makeImage(env.region.size()), {}, cs};
  }
  const double ks = p.specularConstant, exponent = p.specularExponent;
  return lightingPass(env, cs, sub, in, p.light, p.surfaceScale, brushColor(p.lightingColor, env, QColor(Qt::white)),
                      [ks, exponent](Vec3 n, Vec3 l, Rgbf c) {
                        const Vec3 hv = normalized({l.x, l.y, l.z + 1.0});
                        const double f = ks * std::pow(std::max(0.0, dot(n, hv)), exponent);
                        const double r = clamp01(f * c.r), g = clamp01(f * c.g), b = clamp01(f * c.b);
                        return pack(r, g, b, std::max({r, g, b}));
                      });
}

}  // namespace rnqnx::views::rnsvg::filter::detail
