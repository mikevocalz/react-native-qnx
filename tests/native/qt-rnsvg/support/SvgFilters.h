#pragma once

#include "ModelAdapter.h"
#include "XmlDocument.h"

#include <vector>

namespace rnqnx::views::rnsvg::testing {

// The FilterPrimitive elements react-native-svg 15.16.0 mounts for one Fe*
// tag, in order. feDropShadow expands to the five primitives its JS component
// renders (blur, offset, flood, composite, merge). Tags whose component renders
// nothing in 15.16.0 (feImage, feTile, feTurbulence, lighting, morphology,
// convolve, displacement, component transfer) give an empty list. Without the
// full model (commit 112fe48) the list is always empty.
std::vector<ElementProps> buildFilterPrimitives(const XmlNode& node);

bool isFilterPrimitiveTag(const QString& tag);

}  // namespace rnqnx::views::rnsvg::testing
