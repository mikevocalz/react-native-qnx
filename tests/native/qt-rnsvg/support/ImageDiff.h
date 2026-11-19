#pragma once

// Pixel comparison between a Qt rendering and a reference image.
//
// Both images are compared as opaque RGB (the caller composites on white). A
// pixel "differs" when any channel is more than `channelThreshold` away. The
// result carries five numbers because each one catches a different failure:
//   differingPercent  broad drift, such as a wrong color or a shifted shape
//   meanAbsDiff       small differences everywhere, such as blur or gamma
//   largestBlock      one wrong shape in a mostly right picture, since anti-aliasing
//                     noise at edges rarely forms a large connected area
//   shiftTolerant*    the same two counts after forgiving a pixel that matches some
//                     reference pixel within one pixel (and the reverse). Rasterisers
//                     place anti-aliased edge pixels differently, so on long strokes
//                     differingPercent is edge noise; a missing marker, a wrong tile or
//                     a wrong hue survives this forgiveness. Thresholds on these two
//                     numbers can be tight without false failures.

#include <QImage>

namespace rnqnx::views::rnsvg::testing {

struct DiffThresholds {
  int channelThreshold{24};          // of 255
  double maxDiffPercent{1.5};        // of all pixels
  double maxMeanAbsDiff{2.0};        // of 255, averaged over R, G and B
  int maxBlockPixels{400};           // largest 4-connected region of differing pixels
  double maxShiftTolerantPercent{0.1};  // of all pixels, after the one-pixel forgiveness
  int maxShiftTolerantBlock{40};        // largest 4-connected region after the forgiveness
};

struct DiffMetrics {
  bool comparable{false};  // false when the sizes differ or an image is empty or too large
  QString problem;         // why not comparable
  int width{0}, height{0};
  qint64 differing{0};
  double differingPercent{0};
  double meanAbsDiff{0};
  int largestBlock{0};
  qint64 shiftTolerantDiffering{0};
  double shiftTolerantPercent{0};
  int shiftTolerantBlock{0};
};

// Images over this many pixels are refused: the block search keeps one bit per pixel.
inline constexpr qint64 kMaxDiffPixels = 64LL * 1024 * 1024;

DiffMetrics compareImages(const QImage& actual, const QImage& expected, int channelThreshold = 24);

// The metrics against the limits; empty when within them, otherwise a one-line reason.
QString checkThresholds(const DiffMetrics& m, const DiffThresholds& t);

// Red where the pixel differs, a faded copy of the reference elsewhere.
QImage diffImage(const QImage& actual, const QImage& expected, int channelThreshold = 24);

}  // namespace rnqnx::views::rnsvg::testing
