#pragma once

#include "FilterModel.h"

#include <QImage>

namespace rnqnx::views::rnsvg::filter::detail {

// Blurs a premultiplied ARGB32 image in place. The deviations are in device
// pixels along the image axes; a zero or non-finite axis is left alone.
//
// Deviations of 2.0 and up use the three box blurs of the SVG specification,
// below that a sampled Gaussian kernel. Both cost O(pixels) for any deviation
// (running sums; the kernel is at most 13 taps). Edge pixels outside the image
// come from `edge`, applied to each pass.
void gaussianBlur(QImage& image, double sigmaX, double sigmaY, EdgeMode edge);

}  // namespace rnqnx::views::rnsvg::filter::detail
