#pragma once

#include "ImageSource.h"

#include <QByteArray>
#include <QString>

namespace rnqnx::views::rnsvg {

// Decodes encoded image bytes (PNG, JPEG, GIF, ... whatever Qt's image-format
// plugins read) within the renderer's bounds. A raster larger than
// kMaxImageDimension fails before its pixels are allocated.
//
// SVG documents go through the same QImageReader, which reaches them only when
// Qt's `qsvg` image-format plugin is installed at run time. This renderer does
// not link Qt6::Svg. The raster comes out at the document's own size (scaled
// down to the dimension bound), so an SVG enlarged by preserveAspectRatio is
// resampled, not redrawn.
ImageLoadResult decodeImageBytes(const QByteArray& bytes);

// Decodes a `data:` URI, base64 or percent-encoded (react-native-svg authors
// write inline SVG as `data:image/svg+xml;utf8,<svg ...>`).
ImageLoadResult decodeDataUri(const QString& uri);

// True when `bytes` start like an SVG or XML document.
bool looksLikeSvg(const QByteArray& bytes);

// True when a URI names an SVG by media type or file extension.
bool uriNamesSvg(const QString& uri);

// True when this Qt install can read SVG through QImageReader.
bool svgDecodingAvailable();

// The error text for an SVG that cannot be decoded here.
QString svgUnavailableMessage();

}  // namespace rnqnx::views::rnsvg
