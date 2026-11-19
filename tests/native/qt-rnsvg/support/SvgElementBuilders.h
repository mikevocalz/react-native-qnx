#pragma once

// Builds the ElementProps the Fabric bridge would receive for one SVG element,
// following the JS component for that tag (src/elements/*.tsx) and the extract
// helpers it calls. Children are not handled here; SvgDocumentLoader walks them.

#include "ModelAdapter.h"
#include "XmlDocument.h"

#include <optional>

namespace rnqnx::views::rnsvg::testing {

// The element kind react-native-svg mounts for `tag`, or nullopt where the JS
// layer renders nothing: unknown tags, <style>, <title>, and the Fe* primitives
// that 15.16.0 leaves unimplemented. Nested <svg> maps to Group.
std::optional<Kind> kindForTag(const QString& tag);

ElementProps buildElementProps(Kind kind, const XmlNode& node);

// The `d` string of <polygon> and <polyline> (xml.tsx -> Polygon.tsx).
QString polyPointsToPath(const QString& points, bool close);

}  // namespace rnqnx::views::rnsvg::testing
