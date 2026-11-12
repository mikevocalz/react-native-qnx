#pragma once

#include <QString>

namespace rnqnx::views::detail {

// Checks command arity, arc flags and bounded finite numbers before Qt parses
// the path. Supports implicit command repetition and SVG number separators.
bool isSupportedSvgPath(const QString& path);

}  // namespace rnqnx::views::detail
