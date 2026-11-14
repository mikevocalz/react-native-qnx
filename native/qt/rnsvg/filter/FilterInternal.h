#pragma once

// Types shared by the engine and the primitive implementations. Nothing here
// is part of the contract with the scene; include FilterEngine.h for that.

#include "FilterEngine.h"

#include <QImage>
#include <QRect>
#include <QRectF>
#include <QTransform>
#include <cmath>
#include <optional>

namespace rnqnx::views::rnsvg::filter::detail {

constexpr double kPi = 3.14159265358979323846;
// feTurbulence octaves beyond this add less than 2^-24 of amplitude.
constexpr int kMaxOctaves = 24;
// feConvolveMatrix keeps order * order work per pixel bounded.
constexpr int kMaxConvolveOrder = 32;

// One primitive result: a region-sized premultiplied ARGB32 image, the part of
// it the primitive was allowed to write, and the color space its numbers are
// in. Results stay in the space they were produced in and are converted when a
// consumer needs another one.
struct Layer {
  QImage image;
  QRect subregion;  // relative to the filter region's top-left
  ColorSpace space{ColorSpace::SRGB};
};

enum class Axis { X, Y, Diagonal };

// Everything a primitive needs to turn SVG numbers into device pixels.
struct Env {
  const Context* context{nullptr};
  QRect region;           // filter region, device pixels
  QRectF userRegion;      // filter region, user space
  QTransform toDevice;    // user space to device pixels
  QTransform fromDevice;  // inverse of toDevice
  QRectF bbox;
  bool primitiveObb{false};
  double scaleX{1}, scaleY{1};  // lengths of the transformed unit vectors

  // A number given in primitive units (stdDeviation, scale, light position).
  double userNumber(double v, Axis axis) const;
  // A Length given in primitive units (dx, dy, radius).
  double userLength(const Length& l, Axis axis) const;
  // A user-space offset as a device-space vector (applies rotation and flips).
  QPointF vectorToDevice(double dx, double dy) const;
  // Standard deviations or radii along user x and y as extents along device
  // x and y. Exact for axis-aligned and 90 degree transforms; for other
  // rotations it keeps the device-axis marginal variance.
  QPointF extentToDevice(double ux, double uy) const;
};

std::optional<Env> makeEnv(const Definition& definition, const Context& context);

QImage makeImage(const QSize& size);  // zero-filled premultiplied ARGB32
// Clears everything of `image` outside `keep`.
void clipToRect(QImage& image, const QRect& keep);

inline double clamp01(double v) { return v > 0 ? (v < 1 ? v : 1) : 0; }  // NaN gives 0

}  // namespace rnqnx::views::rnsvg::filter::detail
