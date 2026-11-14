#pragma once

// Value types shared by the Image element's loader, its providers and the
// Fabric bridge. Nothing here depends on vnext, Fabric or React.

#include <QImage>
#include <QString>
#include <variant>

namespace rnqnx::views::rnsvg {

// The load state of one Image element's `imageUri`. ElementProps::imageState
// stores the numeric value so RnsvgModel.h stays free of this header.
enum class ImageState : int { Idle = 0, Loading = 1, Loaded = 2, Failed = 3 };

constexpr int toInt(ImageState state) { return static_cast<int>(state); }

// Bounds on what an Image element accepts, applied to every source.
// A raster wider or taller than this fails; an SVG is scaled down to fit.
constexpr int kMaxImageDimension = 8192;
// Characters in a data: URI (the base64 text counts, not the decoded bytes).
constexpr qsizetype kMaxDataUriChars = 16 * 1024 * 1024;

struct ImageLoadFailure {
  QString message;
};

// A provider hands back a decoded image or the reason it has none.
using ImageLoadResult = std::variant<QImage, ImageLoadFailure>;

}  // namespace rnqnx::views::rnsvg
