#include "GaussianBlur.h"

#include "FilterInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

constexpr double kMaxSigma = 1.0e5;

// A line of `n` items of `m` bytes each. Horizontal blur sees a pixel row as
// n items of 4 bytes; vertical blur sees the image as height items of
// 4 * width bytes, so both walk memory in row order.
struct Line {
  const uchar* data;
  qsizetype stride;  // bytes between items
  int n;
  int m;
  const uchar* at(int i) const { return data + stride * i; }
};

int edgeIndex(int i, int n, EdgeMode mode) {
  if (i >= 0 && i < n) return i;
  switch (mode) {
    case EdgeMode::None: return -1;
    case EdgeMode::Duplicate: return i < 0 ? 0 : n - 1;
    case EdgeMode::Wrap: {
      const int r = i % n;
      return r < 0 ? r + n : r;
    }
  }
  return -1;
}

void addItem(qint64* sum, const uchar* item, int m, qint64 weight) {
  if (!item) return;
  for (int c = 0; c < m; ++c) sum[c] += weight * item[c];
}

// Sum of items i-lo .. i+hi for i = 0, in O(n) for any window size.
void initialWindow(const Line& line, int lo, int hi, EdgeMode mode, qint64* sum) {
  const int n = line.n, m = line.m;
  std::fill(sum, sum + m, qint64{0});
  const int insideEnd = std::min(hi, n - 1);
  for (int k = 0; k <= insideEnd; ++k) addItem(sum, line.at(k), m, 1);
  if (mode == EdgeMode::None) return;
  const qint64 leftCount = lo;                              // k = -lo .. -1
  const qint64 rightCount = std::max(0, hi - (n - 1));      // k = n .. hi
  if (mode == EdgeMode::Duplicate) {
    addItem(sum, line.at(0), m, leftCount);
    addItem(sum, line.at(n - 1), m, rightCount);
    return;
  }
  // Wrap: whole cycles contribute the line total, the remainder is a run.
  auto cyclic = [&](qint64 count, bool fromEnd) {
    if (count <= 0) return;
    const qint64 cycles = count / n;
    const int rem = static_cast<int>(count % n);
    if (cycles > 0) {
      for (int k = 0; k < n; ++k) addItem(sum, line.at(k), m, cycles);
    }
    for (int k = 0; k < rem; ++k) addItem(sum, line.at(fromEnd ? n - rem + k : k), m, 1);
  };
  cyclic(leftCount, true);
  cyclic(rightCount, false);
}

// One box pass over a line: out[i] = mean of items i-lo .. i+hi.
void boxPass(const Line& in, uchar* out, qsizetype outStride, int lo, int hi, EdgeMode mode,
             std::vector<qint64>& sum) {
  const int n = in.n, m = in.m;
  const float inv = 1.0f / static_cast<float>(static_cast<qint64>(lo) + hi + 1);
  sum.assign(static_cast<size_t>(m), 0);
  initialWindow(in, lo, hi, mode, sum.data());
  for (int i = 0; i < n; ++i) {
    uchar* o = out + outStride * i;
    for (int c = 0; c < m; ++c) {
      o[c] = static_cast<uchar>(std::min<float>(255.0f, static_cast<float>(sum[static_cast<size_t>(c)]) * inv + 0.5f));
    }
    if (i + 1 < n) {
      const int addIdx = edgeIndex(i + 1 + hi, n, mode);
      const int subIdx = edgeIndex(i - lo, n, mode);
      if (addIdx >= 0) addItem(sum.data(), in.at(addIdx), m, 1);
      if (subIdx >= 0) addItem(sum.data(), in.at(subIdx), m, -1);
    }
  }
}

// One pass of a sampled Gaussian kernel over a line.
void kernelPass(const Line& in, uchar* out, qsizetype outStride, const std::vector<float>& weights, EdgeMode mode,
                std::vector<float>& acc) {
  const int n = in.n, m = in.m;
  const int radius = static_cast<int>(weights.size() / 2);
  acc.resize(static_cast<size_t>(m));
  for (int i = 0; i < n; ++i) {
    std::fill(acc.begin(), acc.end(), 0.0f);
    for (int k = -radius; k <= radius; ++k) {
      const int idx = edgeIndex(i + k, n, mode);
      if (idx < 0) continue;
      const uchar* item = in.at(idx);
      const float w = weights[static_cast<size_t>(k + radius)];
      for (int c = 0; c < m; ++c) acc[static_cast<size_t>(c)] += w * item[c];
    }
    uchar* o = out + outStride * i;
    for (int c = 0; c < m; ++c) o[c] = static_cast<uchar>(std::min(255.0f, acc[static_cast<size_t>(c)] + 0.5f));
  }
}

struct Box {
  int lo, hi;
};

// The SVG box approximation; empty when sigma is below the box threshold.
std::vector<Box> boxesFor(double sigma) {
  if (sigma < 2.0) return {};
  const int d = static_cast<int>(std::floor(sigma * 3.0 * std::sqrt(2.0 * kPi) / 4.0 + 0.5));
  if (d % 2 == 1) return {{(d - 1) / 2, (d - 1) / 2}, {(d - 1) / 2, (d - 1) / 2}, {(d - 1) / 2, (d - 1) / 2}};
  return {{d / 2, d / 2 - 1}, {d / 2 - 1, d / 2}, {d / 2, d / 2}};
}

std::vector<float> kernelFor(double sigma) {
  const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
  std::vector<float> w(static_cast<size_t>(2 * radius + 1));
  double total = 0;
  for (int k = -radius; k <= radius; ++k) {
    const double v = std::exp(-(static_cast<double>(k) * k) / (2.0 * sigma * sigma));
    w[static_cast<size_t>(k + radius)] = static_cast<float>(v);
    total += v;
  }
  for (float& v : w) v = static_cast<float>(v / total);
  return w;
}

// Runs every pass of one axis, ping-ponging between `a` and `b`; returns the
// image that holds the result.
QImage* blurAxis(QImage& a, QImage& b, bool horizontal, double sigma, EdgeMode mode) {
  const std::vector<Box> boxes = boxesFor(sigma);
  const std::vector<float> weights = boxes.empty() ? kernelFor(sigma) : std::vector<float>{};
  const size_t passes = boxes.empty() ? 1 : boxes.size();
  std::vector<qint64> sum;
  std::vector<float> acc;
  QImage* src = &a;
  QImage* dst = &b;
  for (size_t p = 0; p < passes; ++p) {
    if (horizontal) {
      for (int y = 0; y < src->height(); ++y) {
        const Line line{src->constScanLine(y), 4, src->width(), 4};
        if (boxes.empty()) kernelPass(line, dst->scanLine(y), 4, weights, mode, acc);
        else boxPass(line, dst->scanLine(y), 4, boxes[p].lo, boxes[p].hi, mode, sum);
      }
    } else {
      const Line line{src->constBits(), src->bytesPerLine(), src->height(), src->width() * 4};
      // Rows are bytesPerLine apart and 4 * width bytes long.
      if (boxes.empty()) kernelPass(line, dst->bits(), dst->bytesPerLine(), weights, mode, acc);
      else boxPass(line, dst->bits(), dst->bytesPerLine(), boxes[p].lo, boxes[p].hi, mode, sum);
    }
    std::swap(src, dst);
  }
  return src;
}

}  // namespace

void gaussianBlur(QImage& image, double sigmaX, double sigmaY, EdgeMode edge) {
  if (image.isNull()) return;
  auto usable = [](double s) { return std::isfinite(s) && s > 1.0e-3; };
  const bool doX = usable(sigmaX), doY = usable(sigmaY);
  if (!doX && !doY) return;
  QImage scratch(image.size(), image.format());
  QImage* current = &image;
  QImage* other = &scratch;
  if (doX) {
    QImage* result = blurAxis(*current, *other, true, std::min(sigmaX, kMaxSigma), edge);
    if (result != current) std::swap(current, other);
  }
  if (doY) {
    QImage* result = blurAxis(*current, *other, false, std::min(sigmaY, kMaxSigma), edge);
    if (result != current) std::swap(current, other);
  }
  if (current != &image) image = *current;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
