#pragma once

// Test-only loader: parses an SVG document into the retained element tree the
// Fabric bridge would build for the same markup rendered through
// react-native-svg's SvgXml (or SvgCss for documents with a <style> block).
// It uses Qt Core only (QXmlStreamReader), not Qt6::Svg.
//
// The tree is what the renderer paints. Nothing here renders.

#include "ModelAdapter.h"

#include <QSizeF>
#include <QStringList>
#include <memory>

namespace rnqnx::views::rnsvg::testing {

struct LoadOptions {
  // Apply <style> rules the way SvgCss does. SvgXml leaves them alone.
  bool applyStyleSheets{false};
  // Size of the root item in pixels. In an app this comes from layout (the
  // View that React Native sizes from the <Svg> width and height, or its
  // container); the fixture runner passes the reference image's size.
  QSizeF itemSize{100, 100};
};

struct LoadedDocument {
  std::unique_ptr<RnsvgRootItem> root;  // null when `error` is set
  QString error;
  // <svg opacity>, which React Native applies to the whole view, not the renderer.
  double rootOpacity{1.0};
  // Tags that produced no element: unknown to react-native-svg, unimplemented in
  // 15.16.0 (Fe* primitives without a native component), or beyond the model.
  QStringList skipped;
};

LoadedDocument loadSvgDocument(const QByteArray& xml, const LoadOptions& options = {});

}  // namespace rnqnx::views::rnsvg::testing
