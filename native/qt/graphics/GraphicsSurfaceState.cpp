#include "GraphicsSurfaceState.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace rnqnx::graphics {

GraphicsViewport GraphicsViewport::fromLogicalSize(QSize size, qreal dpr) {
  if (size.width() < 0 || size.height() < 0 || !std::isfinite(dpr) || dpr <= 0) {
    throw std::invalid_argument("Graphics viewport requires nonnegative dimensions and a finite positive DPR.");
  }
  const auto physical = [dpr](int dimension) {
    const double value = std::round(static_cast<double>(dimension) * dpr);
    if (!std::isfinite(value) || value > std::numeric_limits<int>::max()) {
      throw std::overflow_error("Graphics viewport physical size exceeds the supported integer range.");
    }
    return static_cast<int>(value);
  };
  return {size, QSize(physical(size.width()), physical(size.height())), dpr};
}

}  // namespace rnqnx::graphics
