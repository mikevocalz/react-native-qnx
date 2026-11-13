#pragma once

// Applies a list of filter primitives to a rendered source graphic.
//
// The scene resolves everything that needs the element tree (which elements
// make up the filter, the element's bounding box, its transform) and hands the
// engine device-pixel work: the engine has no knowledge of QQuickItem or of
// Fabric. This header is the contract between the two.
//
// How the engine reads the specifications (SVG 1.1, Filter Effects Level 1):
//  - Pixels are premultiplied ARGB32. Each primitive result keeps the color
//    space it was produced in (color-interpolation-filters, linearRGB by
//    default) and is converted, with an 8-bit table, only when a consumer
//    needs the other one. The final image is sRGB.
//  - A transform that rotates or skews is not undone. The filter runs on a
//    device-aligned canvas over the bounding box of the transformed region;
//    lengths are scaled by the transform (offsets and displacements follow the
//    transformed vector, deviations and radii use the per-axis extent).
//  - A filter that cannot be applied (singular transform, region with no area,
//    objectBoundingBox units on an element without a box, region over
//    kMaxRegionPixels) gives `Result::valid == false`; the element is then not
//    drawn, as in SVG 1.1 and resvg. A primitive with bad parameters does not
//    fail the filter; its own rule (input passes through, or transparent
//    black) is documented with the primitive.

#include "FilterModel.h"

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include <functional>
#include <vector>

namespace rnqnx::views::rnsvg::filter {

// The filter region may cover at most this many device pixels. A premultiplied
// ARGB32 image of that size is 32 MiB and the engine keeps a few alive at
// once. A larger region is reported as invalid instead of being scaled down,
// because the scene renders the element into exactly regionInDevice().
constexpr qint64 kMaxRegionPixels = qint64{8} * 1024 * 1024;

// The <filter> element's own attributes.
struct FilterRegion {
  Length x{-10, Length::Unit::Percent, true};
  Length y{-10, Length::Unit::Percent, true};
  Length width{120, Length::Unit::Percent, true};
  Length height{120, Length::Unit::Percent, true};
  bool filterUserSpace{false};     // filterUnits="userSpaceOnUse"
  bool primitiveUserSpace{true};   // primitiveUnits="userSpaceOnUse"
};

struct Definition {
  FilterRegion region;
  // Non-owning; the elements outlive one apply() call. A null entry is
  // skipped. Each primitive's `params` alternative decides what it does;
  // `Primitive::type` is informational.
  std::vector<const Primitive*> primitives;
};

struct Context {
  QTransform userToDevice;  // the filtered element's user space to device pixels
  QRectF objectBoundingBox;  // user space; empty when the element has no geometry
  QSizeF viewport;           // user space size, for percentage lengths
  double fontSize{12};       // for em lengths
  QColor currentColor{Qt::black};
  // FillPaint and StrokePaint inputs: the element's flat paint color, which
  // covers the whole filter region. An invalid color gives an empty image.
  // (resvg 0.50 substitutes SourceGraphic for both; this follows SVG 1.1.)
  QColor fillPaint, strokePaint;

  // feImage with an element reference: renders the element `id` into an image
  // covering `deviceRect`, drawn with `userToDevice`. Empty image: not found.
  std::function<QImage(const QString& id, const QTransform& userToDevice, const QRect& deviceRect)> renderElement;
  // feImage with a URL (data:, file:, bundled asset): decoded image, or null.
  std::function<QImage(const QString& href)> loadImage;

  // Device pixels the caller can show, with margin. A region larger than
  // kMaxRegionPixels (width="180" in objectBoundingBox units is 180 boxes
  // wide) is cut to this rectangle instead of being invalid; the primitives
  // still resolve against the full region. Null: no cut, such a region is
  // invalid.
  QRect workArea;
};

// The filter region in device pixels: the origin is floored and the size
// rounded up, the way resvg snaps it. The scene renders the element into an
// image of exactly this rectangle. Empty when the filter cannot be applied
// (see the notes above), in which case the element is not drawn.
QRect regionInDevice(const Definition& definition, const Context& context);

struct Result {
  QImage image;   // premultiplied ARGB32, sRGB; covers `rect`
  QRect rect;     // device pixels
  bool valid{false};  // false: draw nothing for this element
};

// `source` covers regionInDevice() and holds the element's rendering,
// premultiplied ARGB32 with the origin at the region's top-left. A source of
// another size or format is drawn into a region-sized image first. `source` is
// never modified. BackgroundImage and BackgroundAlpha read as transparent
// images; an empty primitive list gives a valid, fully transparent result.
Result apply(const Definition& definition, const Context& context, const QImage& source);

}  // namespace rnqnx::views::rnsvg::filter
