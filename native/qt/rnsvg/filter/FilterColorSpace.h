#pragma once

#include "FilterModel.h"
#include "PixelOps.h"

#include <QColor>
#include <QImage>
#include <QRgb>

namespace rnqnx::views::rnsvg::filter::detail {

// Converts a premultiplied ARGB32 image between sRGB and linearRGB in place.
// Channels go through an 8-bit table on demultiplied values, the way resvg
// does it, so a round trip loses precision in the darkest steps.
void convertColorSpace(QImage& image, ColorSpace from, ColorSpace to);

// Same, on a copy; returns `image` unchanged when the spaces match.
QImage inColorSpace(const QImage& image, ColorSpace from, ColorSpace to);

// A color given in sRGB, as the numbers a primitive working in `space` uses.
// Returns a premultiplied pixel.
QRgb premultipliedIn(const QColor& srgb, double opacity, ColorSpace space);

}  // namespace rnqnx::views::rnsvg::filter::detail
