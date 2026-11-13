#include "react-native-qnx/views/image/ImageEffects.h"

#include <QPainter>

#include <algorithm>
#include <vector>

namespace rnqnx::views {
namespace {

// One running-sum pass along rows (horizontal) or columns, on 4 channels.
void boxPass(QImage& image, int radius, bool horizontal) {
  const int width = image.width();
  const int height = image.height();
  const int lines = horizontal ? height : width;
  const int length = horizontal ? width : height;
  const int window = 2 * radius + 1;
  std::vector<quint32> line(static_cast<size_t>(length));
  for (int row = 0; row < lines; ++row) {
    for (int i = 0; i < length; ++i) {
      line[static_cast<size_t>(i)] = horizontal
          ? reinterpret_cast<const quint32*>(image.constScanLine(row))[i]
          : reinterpret_cast<const quint32*>(image.constScanLine(i))[row];
    }
    quint32 sums[4] = {0, 0, 0, 0};
    const auto add = [&](int index, int sign) {
      const int clamped = std::clamp(index, 0, length - 1);
      const quint32 pixel = line[static_cast<size_t>(clamped)];
      for (int c = 0; c < 4; ++c) {
        const quint32 part = (pixel >> (8 * c)) & 0xff;
        sums[c] = sign > 0 ? sums[c] + part : sums[c] - part;
      }
    };
    for (int i = -radius; i <= radius; ++i) add(i, +1);
    for (int i = 0; i < length; ++i) {
      quint32 out = 0;
      for (int c = 0; c < 4; ++c) out |= ((sums[c] + static_cast<quint32>(window) / 2) / static_cast<quint32>(window)) << (8 * c);
      if (horizontal) reinterpret_cast<quint32*>(image.scanLine(row))[i] = out;
      else reinterpret_cast<quint32*>(image.scanLine(i))[row] = out;
      add(i - radius, -1);
      add(i + radius + 1, +1);
    }
  }
}

}  // namespace

QImage blurImage(const QImage& source, int radius) {
  if (radius <= 0 || source.isNull()) return source;
  QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  // Three passes of a box of about radius/3 approximate a Gaussian of that sigma.
  const int pass = std::max(1, radius / 2);
  for (int i = 0; i < 3; ++i) {
    boxPass(image, pass, true);
    boxPass(image, pass, false);
  }
  return image;
}

QImage tintImage(const QImage& source, const QColor& color) {
  if (source.isNull()) return source;
  QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&image);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(image.rect(), color);
  return image;
}

}  // namespace rnqnx::views
