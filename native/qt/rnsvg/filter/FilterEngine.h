#pragma once

// Applies a list of filter primitives to a rendered source graphic.
//
// The scene resolves everything that needs the element tree (which elements
// make up the filter, the element's bounding box, its transform) and hands the
// engine device-pixel work: the engine has no knowledge of QQuickItem or of
// Fabric. This header is the contract between the two.

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
  // Non-owning; the elements outlive one apply() call.
  std::vector<const Primitive*> primitives;
};

struct Context {
  QTransform userToDevice;  // the filtered element's user space to device pixels
  QRectF objectBoundingBox;  // user space; empty when the element has no geometry
  QSizeF viewport;           // user space size, for percentage lengths
  double fontSize{12};       // for em lengths
  QColor currentColor{Qt::black};
  QColor fillPaint, strokePaint;  // flat colors of the element, when it has them

  // feImage with an element reference: renders the element `id` into an image
  // covering `deviceRect`, drawn with `userToDevice`. Empty image: not found.
  std::function<QImage(const QString& id, const QTransform& userToDevice, const QRect& deviceRect)> renderElement;
  // feImage with a URL (data:, file:, bundled asset): decoded image, or null.
  std::function<QImage(const QString& href)> loadImage;
};

// The filter region in device pixels, rounded out to whole pixels. The scene
// renders the element into an image of exactly this rectangle. Empty when the
// region is degenerate, in which case SVG says the element is not rendered.
QRect regionInDevice(const Definition& definition, const Context& context);

struct Result {
  QImage image;   // premultiplied ARGB32; covers `rect`
  QRect rect;     // device pixels
  bool valid{false};
};

// `source` covers regionInDevice() and holds the element's rendering,
// premultiplied ARGB32 with the origin at the region's top-left.
Result apply(const Definition& definition, const Context& context, const QImage& source);

}  // namespace rnqnx::views::rnsvg::filter
