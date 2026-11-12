#pragma once

#include <QList>
#include <QString>

namespace rnqnx::identity {

inline constexpr int kMinimumIconSourcePx = 1024;
inline constexpr int kMaximumIconSourcePx = 8192;

struct GeneratedIcon {
  int size;
  QString path;
};

// Scales one square PNG source of at least 1024x1024 to every requested edge
// with Qt's smooth (area-averaging) scaler and writes icon-<size>.png files
// into outputDirectory. Corners stay square: no mask is applied. Throws
// std::runtime_error naming the file and the fix when the source is rejected.
QList<GeneratedIcon> writeIconSet(
    const QString& sourcePng, const QString& outputDirectory, const QList<int>& sizes);

} // namespace rnqnx::identity
