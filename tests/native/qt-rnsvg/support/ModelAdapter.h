#pragma once

// The one place where the test loader touches the shape of the retained model.
// The model gained fields (Pattern, Symbol, Marker, TextPath, Filter, Fe*,
// ForeignObject, grouped text props) after commit 112fe48. The fixture runner
// builds against either version, so everything that differs lives here and the
// rest of the loader sees one interface.

#include "RnsvgItems.h"
#include "RnsvgModel.h"

#include <QPainterPath>
#include <QString>

#include <vector>

#if __has_include("RnsvgBasicTypes.h")
#define RNQNX_RNSVG_FULL_MODEL 1
#else
#define RNQNX_RNSVG_FULL_MODEL 0
#endif

namespace rnqnx::views::rnsvg::testing {

// clipRule as react-native-svg extracts it: -1 unset, 0 evenodd, 1 nonzero.
inline void setClipRule(ElementProps& e, int rule) {
#if RNQNX_RNSVG_FULL_MODEL
  e.clipRule = rule;
#else
  e.clipEvenOdd = rule == 0;
#endif
}

// Per-character position lists of Text and TSpan (x, y, dx, dy).
inline void setTextPositions(ElementProps& e, std::vector<Length> x, std::vector<Length> y, std::vector<Length> dx,
                             std::vector<Length> dy) {
#if RNQNX_RNSVG_FULL_MODEL
  e.text.x = std::move(x);
  e.text.y = std::move(y);
  e.text.dx = std::move(dx);
  e.text.dy = std::move(dy);
#else
  e.tx = std::move(x);
  e.ty = std::move(y);
  e.tdx = std::move(dx);
  e.tdy = std::move(dy);
#endif
}

// True when the model can hold `kind`. The old model stops at Image.
inline constexpr bool modelHasFullKinds() { return RNQNX_RNSVG_FULL_MODEL != 0; }

}  // namespace rnqnx::views::rnsvg::testing

namespace rnqnx::views::rnsvg::testing {

// Parses `d` into e.path and, where the model keeps them, e.vertices.
QPainterPath parsePathInto(ElementProps& e, const QString& d);

}  // namespace rnqnx::views::rnsvg::testing
