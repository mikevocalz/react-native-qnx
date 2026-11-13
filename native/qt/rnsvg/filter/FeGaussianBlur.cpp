#include "FilterColorSpace.h"
#include "FilterPrimitives.h"
#include "GaussianBlur.h"

namespace rnqnx::views::rnsvg::filter::detail {

// Filter Effects Level 1: a negative or non-finite deviation, or two zero
// deviations, disable the primitive and its result is the input image. One
// zero deviation blurs along the other axis only. SVG 1.1 called a negative
// value an error; resvg and browsers follow the newer text, so does this.
Layer applyGaussianBlur(const Env& env, const GaussianBlur& p, ColorSpace cs, const Layer& in) {
  const double sx = p.stdDeviationX, sy = p.stdDeviationY;
  if (!std::isfinite(sx) || !std::isfinite(sy) || sx < 0 || sy < 0 || (sx == 0 && sy == 0)) return in;
  const QPointF dev = env.extentToDevice(env.userNumber(sx, Axis::X), env.userNumber(sy, Axis::Y));
  QImage image = inColorSpace(in.image, in.space, cs);
  gaussianBlur(image, dev.x(), dev.y(), p.edgeMode);
  return {image, {}, cs};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
