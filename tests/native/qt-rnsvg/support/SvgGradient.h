#pragma once

#include "ModelAdapter.h"
#include "XmlDocument.h"

namespace rnqnx::views::rnsvg::testing {

// LinearGradient.tsx and RadialGradient.tsx plus extractGradient(): geometry
// lengths, units, gradientTransform and the sorted stop list with each stop's
// alpha taken from stop-opacity.
ElementProps buildGradient(Kind kind, const XmlNode& node);

}  // namespace rnqnx::views::rnsvg::testing
