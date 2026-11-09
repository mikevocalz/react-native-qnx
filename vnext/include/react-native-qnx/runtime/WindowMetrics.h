#pragma once

#include <QSizeF>

namespace rnqnx::runtime {

/// Internal host observations in Qt/React logical units. Pixel ratios are
/// conversion factors, never multipliers already applied to these sizes.
struct WindowMetrics {
  QSizeF windowSize = {0, 0};
  QSizeF screenSize = {0, 0};
  double windowScale = 1.0;
  double screenScale = 1.0;
  // The host's explicit baseline is 1 until an actual text-scale provider
  // supplies a different value. It is independent of display pixel density.
  double fontScale = 1.0;

  bool operator==(const WindowMetrics&) const = default;
};

}  // namespace rnqnx::runtime
