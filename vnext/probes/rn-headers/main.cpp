// Proves that installed React Native 0.86.2 renderer headers compile in this
// repository against the fetched third-party dependencies, and that the fields
// the mounting adapter needs are present with the types we mirrored.
//
// Compiling is the whole point: every field access here fails to build if the
// installed header disagrees with docs/cited-seams.md.

#include <react/renderer/core/LayoutMetrics.h>
#include <react/renderer/core/ReactPrimitives.h>
#include <react/renderer/mounting/ShadowView.h>
#include <react/renderer/mounting/ShadowViewMutation.h>

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"

#include <cstdio>
#include <type_traits>

namespace {

using facebook::react::LayoutMetrics;
using facebook::react::ShadowView;
using facebook::react::ShadowViewMutation;
using facebook::react::Tag;

// The mirrors recorded in docs/cited-seams.md, enforced by the compiler.
static_assert(
    std::is_same_v<Tag, rnqnx::fabric::Tag>,
    "rnqnx::fabric::Tag must match facebook::react::Tag");
static_assert(
    std::is_same_v<decltype(ShadowViewMutation::parentTag), Tag>,
    "ShadowViewMutation::parentTag must be a Tag");
static_assert(
    std::is_same_v<decltype(ShadowViewMutation::index), int>,
    "ShadowViewMutation::index must be an int");
static_assert(
    std::is_same_v<decltype(ShadowView::tag), Tag>,
    "ShadowView::tag must be a Tag");
static_assert(
    std::is_same_v<
        decltype(LayoutMetrics::frame.origin.x),
        decltype(rnqnx::fabric::LayoutFrame::x)>,
    "LayoutFrame must use the same Float type as LayoutMetrics::frame");

// The five mutation types the mounting layer implements.
static_assert(ShadowViewMutation::Type::Create == 1);
static_assert(ShadowViewMutation::Type::Delete == 2);
static_assert(ShadowViewMutation::Type::Insert == 4);
static_assert(ShadowViewMutation::Type::Remove == 8);
static_assert(ShadowViewMutation::Type::Update == 16);

rnqnx::fabric::LayoutFrame toLayoutFrame(const LayoutMetrics& metrics) {
  return rnqnx::fabric::LayoutFrame{
      .x = metrics.frame.origin.x,
      .y = metrics.frame.origin.y,
      .width = metrics.frame.size.width,
      .height = metrics.frame.size.height,
  };
}

}  // namespace

int main() {
  ShadowView view;
  view.tag = 42;
  view.layoutMetrics.frame.origin.x = 10.0F;
  view.layoutMetrics.frame.origin.y = 20.0F;
  view.layoutMetrics.frame.size.width = 100.0F;
  view.layoutMetrics.frame.size.height = 50.0F;

  const rnqnx::fabric::LayoutFrame frame = toLayoutFrame(view.layoutMetrics);

  std::printf(
      "RNQNX_RN_HEADERS_OK tag=%d frame=%.1f,%.1f,%.1fx%.1f\n",
      view.tag,
      static_cast<double>(frame.x),
      static_cast<double>(frame.y),
      static_cast<double>(frame.width),
      static_cast<double>(frame.height));
  return 0;
}
