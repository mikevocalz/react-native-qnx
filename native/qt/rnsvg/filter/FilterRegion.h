#pragma once

#include "FilterInternal.h"

namespace rnqnx::views::rnsvg::filter::detail {

// A primitive subregion in user space. Unset attributes take the filter
// region's value (Filter Effects Level 1: 0%, 0%, 100%, 100% of the region).
// Empty optional: a value was not finite.
std::optional<QRectF> resolveSubregion(const Subregion& sub, const Env& env);

// Maps a user-space rectangle to device pixels: origin floored, size rounded
// up. Empty rect when
// the result is not finite, is wildly out of range or has no area.
QRect toDeviceRect(const QRectF& user, const QTransform& toDevice);

}  // namespace rnqnx::views::rnsvg::filter::detail
