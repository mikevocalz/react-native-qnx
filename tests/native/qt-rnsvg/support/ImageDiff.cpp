#include "ImageDiff.h"

#include <QColor>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace rnqnx::views::rnsvg::testing {
namespace {

QImage asRgb32(const QImage& in) { return in.format() == QImage::Format_RGB32 ? in : in.convertToFormat(QImage::Format_RGB32); }

int maxChannelDiff(QRgb a, QRgb b) {
  return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

// Largest 4-connected region of pixels marked 1 in `mask` (w x h). The marks are
// rewritten to 2 as regions are consumed. An explicit stack keeps the depth
// independent of the region size.
int largestRegion(std::vector<unsigned char>& mask, int w, int h) {
  int best = 0;
  std::vector<qint32> stack;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t index = size_t(y) * size_t(w) + size_t(x);
      if (mask[index] != 1) continue;
      int size = 0;
      mask[index] = 2;
      stack.push_back(qint32(index));
      while (!stack.empty()) {
        const qint32 cur = stack.back();
        stack.pop_back();
        ++size;
        const int cx = cur % w, cy = cur / w;
        const int nx[4] = {cx - 1, cx + 1, cx, cx};
        const int ny[4] = {cy, cy, cy - 1, cy + 1};
        for (int k = 0; k < 4; ++k) {
          if (nx[k] < 0 || ny[k] < 0 || nx[k] >= w || ny[k] >= h) continue;
          const size_t ni = size_t(ny[k]) * size_t(w) + size_t(nx[k]);
          if (mask[ni] == 1) {
            mask[ni] = 2;
            stack.push_back(qint32(ni));
          }
        }
      }
      best = std::max(best, size);
    }
  }
  return best;
}

// True when pixel (x, y) of `a` is within `threshold` of some pixel in the 3x3 neighbourhood of (x, y) in `b`.
bool matchesNearby(const QImage& a, const QImage& b, int x, int y, int threshold) {
  const QRgb pixel = reinterpret_cast<const QRgb*>(a.constScanLine(y))[x];
  for (int dy = -1; dy <= 1; ++dy) {
    const int yy = y + dy;
    if (yy < 0 || yy >= b.height()) continue;
    const auto* row = reinterpret_cast<const QRgb*>(b.constScanLine(yy));
    for (int dx = -1; dx <= 1; ++dx) {
      const int xx = x + dx;
      if (xx < 0 || xx >= b.width()) continue;
      if (maxChannelDiff(pixel, row[xx]) <= threshold) return true;
    }
  }
  return false;
}

}  // namespace

DiffMetrics compareImages(const QImage& actualIn, const QImage& expectedIn, int channelThreshold) {
  DiffMetrics m;
  if (actualIn.isNull() || expectedIn.isNull()) {
    m.problem = QStringLiteral("an image is empty");
    return m;
  }
  if (actualIn.size() != expectedIn.size()) {
    m.problem = QStringLiteral("size %1x%2 differs from the reference %3x%4")
                    .arg(actualIn.width()).arg(actualIn.height()).arg(expectedIn.width()).arg(expectedIn.height());
    return m;
  }
  const qint64 pixels = qint64(actualIn.width()) * actualIn.height();
  if (pixels > kMaxDiffPixels) {
    m.problem = QStringLiteral("image has %1 pixels, over the %2 limit").arg(pixels).arg(kMaxDiffPixels);
    return m;
  }
  const QImage actual = asRgb32(actualIn), expected = asRgb32(expectedIn);
  const int w = actual.width(), h = actual.height();
  m.comparable = true;
  m.width = w;
  m.height = h;

  std::vector<unsigned char> differs(size_t(pixels), 0);
  qint64 sum = 0;
  for (int y = 0; y < h; ++y) {
    const auto* a = reinterpret_cast<const QRgb*>(actual.constScanLine(y));
    const auto* b = reinterpret_cast<const QRgb*>(expected.constScanLine(y));
    for (int x = 0; x < w; ++x) {
      sum += std::abs(qRed(a[x]) - qRed(b[x])) + std::abs(qGreen(a[x]) - qGreen(b[x])) + std::abs(qBlue(a[x]) - qBlue(b[x]));
      if (maxChannelDiff(a[x], b[x]) > channelThreshold) {
        differs[size_t(y) * size_t(w) + size_t(x)] = 1;
        ++m.differing;
      }
    }
  }
  m.differingPercent = 100.0 * double(m.differing) / double(pixels);
  m.meanAbsDiff = double(sum) / (3.0 * double(pixels));

  // The forgiving pass runs on a copy, so the strict block count sees the strict mask.
  std::vector<unsigned char> tolerant(size_t(pixels), 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t index = size_t(y) * size_t(w) + size_t(x);
      if (differs[index] == 1 && !matchesNearby(actual, expected, x, y, channelThreshold) &&
          !matchesNearby(expected, actual, x, y, channelThreshold)) {
        tolerant[index] = 1;
        ++m.shiftTolerantDiffering;
      }
    }
  }
  m.shiftTolerantPercent = 100.0 * double(m.shiftTolerantDiffering) / double(pixels);
  m.shiftTolerantBlock = largestRegion(tolerant, w, h);
  m.largestBlock = largestRegion(differs, w, h);
  return m;
}

QString checkThresholds(const DiffMetrics& m, const DiffThresholds& t) {
  if (!m.comparable) return m.problem;
  QStringList reasons;
  if (m.differingPercent > t.maxDiffPercent) {
    reasons << QStringLiteral("%1% of pixels differ by more than %2 (limit %3%)").arg(m.differingPercent, 0, 'f', 2).arg(t.channelThreshold).arg(t.maxDiffPercent);
  }
  if (m.meanAbsDiff > t.maxMeanAbsDiff) {
    reasons << QStringLiteral("mean difference %1 (limit %2)").arg(m.meanAbsDiff, 0, 'f', 3).arg(t.maxMeanAbsDiff);
  }
  if (m.largestBlock > t.maxBlockPixels) {
    reasons << QStringLiteral("largest differing block %1 px (limit %2)").arg(m.largestBlock).arg(t.maxBlockPixels);
  }
  if (m.shiftTolerantPercent > t.maxShiftTolerantPercent) {
    reasons << QStringLiteral("%1% of pixels match nothing within one pixel (limit %2%)").arg(m.shiftTolerantPercent, 0, 'f', 3).arg(t.maxShiftTolerantPercent);
  }
  if (m.shiftTolerantBlock > t.maxShiftTolerantBlock) {
    reasons << QStringLiteral("largest region matching nothing within one pixel is %1 px (limit %2)").arg(m.shiftTolerantBlock).arg(t.maxShiftTolerantBlock);
  }
  return reasons.join(QStringLiteral("; "));
}

QImage diffImage(const QImage& actualIn, const QImage& expectedIn, int channelThreshold) {
  if (actualIn.isNull() || expectedIn.isNull() || actualIn.size() != expectedIn.size()) return {};
  const QImage actual = asRgb32(actualIn), expected = asRgb32(expectedIn);
  QImage out(actual.size(), QImage::Format_RGB32);
  for (int y = 0; y < actual.height(); ++y) {
    const auto* a = reinterpret_cast<const QRgb*>(actual.constScanLine(y));
    const auto* b = reinterpret_cast<const QRgb*>(expected.constScanLine(y));
    auto* o = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < actual.width(); ++x) {
      if (maxChannelDiff(a[x], b[x]) > channelThreshold) {
        o[x] = qRgb(230, 20, 20);
      } else {
        const int gray = (qGray(b[x]) + 3 * 255) / 4;
        o[x] = qRgb(gray, gray, gray);
      }
    }
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::testing
