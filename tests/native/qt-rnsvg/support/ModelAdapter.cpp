#include "ModelAdapter.h"

#include "RnsvgPath.h"

namespace rnqnx::views::rnsvg::testing {

QPainterPath parsePathInto(ElementProps& e, const QString& d) {
#if RNQNX_RNSVG_FULL_MODEL
  return parseSvgPath(d, nullptr, &e.vertices);
#else
  (void)e;
  return parseSvgPath(d, nullptr);
#endif
}

}  // namespace rnqnx::views::rnsvg::testing
