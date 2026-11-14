#pragma once

#include "ModelAdapter.h"
#include "PropBag.h"

namespace rnqnx::views::rnsvg::testing {

// extractProps(): fill, stroke, opacity, transform, markers, clip, mask,
// filter, color, display, id. Fills in `e` and its propList.
void extractCommonProps(ElementProps& e, const PropBag& p);

// extractFont(): the font props of a text-bearing element, including the
// `font` shorthand.
FontSpec extractFont(const PropBag& p);

// extractTransform() for `transform` (string form).
std::optional<QTransform> extractMatrix(const PropBag& p, const char* name = "transform");

// A Length from an optional prop, or `fallback` (set) when the prop is absent.
Length lengthProp(const PropBag& p, const char* name, Length fallback = {});
Length lengthPx(double v);
Length lengthPercent(double v);

}  // namespace rnqnx::views::rnsvg::testing
