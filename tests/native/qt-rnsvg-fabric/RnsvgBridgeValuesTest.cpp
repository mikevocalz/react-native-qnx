// Numbers JavaScript can send that a C++ cast would turn into undefined
// behaviour or a thrown range_error: NaN, infinities, huge and negative values.
// Links the bridge's value converters directly; no JS runtime involved.
#include "QtRnsvgBridgeInternal.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  using namespace rnqnx::fabric::rnsvg_bridge;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double values[] = {nan, inf, -inf, 1e30, -1e30, 4294967296.0, -4294967297.0, -1.0, 0.0};

  for (double v : values) {
    // None of these may throw or trap; the result only has to be a colour.
    check(colorFromArgb(v).isValid(), "colorFromArgb stays a valid color");
    folly::dynamic type = folly::dynamic::object("type", v);
    try {
      (void)toBrush(type);
      (void)toBrush(folly::dynamic(v));
      (void)toWeight(folly::dynamic(v));
    } catch (const std::exception& e) {
      std::cerr << "FAIL: threw on " << v << ": " << e.what() << '\n';
      ++failures;
    }
  }

  check(colorFromArgb(-16777216.0) == QColor(0, 0, 0, 255), "signed ARGB black is opaque black");
  check(colorFromArgb(4294967295.0) == QColor(255, 255, 255, 255), "unsigned ARGB white");
  check(colorFromArgb(1e30) == QColor::fromRgba(0xFFFFFFFFu), "huge value clamps to white");
  check(toBrush(folly::dynamic::object("type", 1e30)).type == Brush::Type::None, "huge brush type is none");
  check(toBrush(folly::dynamic::object("type", nan)).type == Brush::Type::None, "NaN brush type is none");
  check(toBrush(folly::dynamic::object("type", -5)).type == Brush::Type::None, "negative brush type is none");
  check(toBrush(folly::dynamic::object("type", 2)).type == Brush::Type::CurrentColor, "type 2 is currentColor");

  check(!toWeight(folly::dynamic(nan)).has_value(), "NaN weight is unset");
  check(!toWeight(folly::dynamic(inf)).has_value(), "infinite weight is unset");
  check(toWeight(folly::dynamic(1e20)) == 1000, "huge weight clamps to 1000");
  check(toWeight(folly::dynamic(-50.0)) == 1, "negative weight clamps to 1");
  check(toWeight(folly::dynamic(700)) == 700, "plain weight passes through");
  check(toWeight(folly::dynamic("1e30")) == 1000, "huge numeric string clamps");
  check(toWeight(folly::dynamic("bold")) == 700, "bold is 700");

  check(!makeViewBox(nan, 0, 10, 10, "", 0).set, "NaN minX is no viewBox");
  check(!makeViewBox(0, 0, inf, 10, "", 0).set, "infinite width is no viewBox");
  check(!makeViewBox(0, 0, -10, 10, "", 0).set, "negative width is no viewBox");
  check(makeViewBox(0, 0, 10, 10, "xMidYMid", 99).meetOrSlice == 0, "bad meetOrSlice falls back to meet");

  rnqnx::views::rnsvg::RootProps root;
  check(!root.hasViewBox(), "default root has no viewBox");
  root.vbWidth = 10;
  root.vbHeight = 10;
  check(root.hasViewBox(), "finite positive root viewBox");
  root.minX = nan;
  check(!root.hasViewBox(), "NaN origin is no viewBox");
  root.minX = 0;
  root.vbHeight = inf;
  check(!root.hasViewBox(), "infinite height is no viewBox");
  root.vbHeight = -1;
  check(!root.hasViewBox(), "negative height is no viewBox");

  if (failures == 0) std::cout << "rnsvg bridge values: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
