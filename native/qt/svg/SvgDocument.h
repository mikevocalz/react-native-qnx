#pragma once

#include <QRectF>
#include <QSizeF>
#include <QtGlobal>

namespace rnqnx::views {

enum class SvgResizeMode { Contain, Cover, Stretch };
enum class SvgDocumentState { Empty, Ready, Error };

struct SvgDocumentInfo {
  QSizeF intrinsicSize;
  QRectF viewBox;
};

inline constexpr qsizetype kSvgMaxDocumentBytes = 64 * 1024;
inline constexpr int kSvgMaxElements = 512;
inline constexpr int kSvgMaxDepth = 32;
inline constexpr int kSvgMaxAttributes = 32;
inline constexpr int kSvgMaxAttributeCharacters = 16 * 1024;
inline constexpr qreal kSvgMinDimension = 0.000001;
inline constexpr qreal kSvgMaxDimension = 16384.0;
inline constexpr qreal kSvgMaxCoordinate = 1000000.0;
inline constexpr int kSvgMaxTexturePixels = 4096;

}  // namespace rnqnx::views
