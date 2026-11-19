#include "FilterPrimitives.h"

#include <algorithm>
#include <vector>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// Sliding-window extreme over a window of 2r+1 samples, in O(n) for any r
// (van Herk / Gil-Werman). Samples outside the line are transparent black, so
// an erosion pulls the shape in from the region edge too.
class WindowExtreme {
 public:
  WindowExtreme(bool erode, int n, int r) : erode_(erode), n_(n), r_(r) {
    window_ = 2 * r + 1;
    const int padded = n + 2 * r;
    const int blocks = (padded + window_ - 1) / window_;
    line_.assign(static_cast<size_t>(blocks) * window_, 0);
    prefix_.assign(line_.size(), 0);
    suffix_.assign(line_.size(), 0);
  }

  // `in` has `n` samples `inStride` bytes apart; same for `out`.
  void run(const uchar* in, qsizetype inStride, uchar* out, qsizetype outStride) {
    for (int i = 0; i < n_; ++i) line_[static_cast<size_t>(r_ + i)] = in[inStride * i];
    const int total = static_cast<int>(line_.size());
    for (int start = 0; start < total; start += window_) {
      prefix_[static_cast<size_t>(start)] = line_[static_cast<size_t>(start)];
      for (int j = 1; j < window_; ++j) {
        prefix_[static_cast<size_t>(start + j)] = pick(prefix_[static_cast<size_t>(start + j - 1)], line_[static_cast<size_t>(start + j)]);
      }
      suffix_[static_cast<size_t>(start + window_ - 1)] = line_[static_cast<size_t>(start + window_ - 1)];
      for (int j = window_ - 2; j >= 0; --j) {
        suffix_[static_cast<size_t>(start + j)] = pick(suffix_[static_cast<size_t>(start + j + 1)], line_[static_cast<size_t>(start + j)]);
      }
    }
    for (int i = 0; i < n_; ++i) {
      out[outStride * i] = pick(suffix_[static_cast<size_t>(i)], prefix_[static_cast<size_t>(i + window_ - 1)]);
    }
  }

 private:
  uchar pick(uchar a, uchar b) const { return erode_ ? std::min(a, b) : std::max(a, b); }
  bool erode_;
  int n_, r_, window_;
  std::vector<uchar> line_, prefix_, suffix_;
};

void passRows(const QImage& in, QImage& out, bool erode, int r) {
  WindowExtreme w(erode, in.width(), std::min(r, in.width()));
  for (int y = 0; y < in.height(); ++y) {
    for (int c = 0; c < 4; ++c) w.run(in.constScanLine(y) + c, 4, out.scanLine(y) + c, 4);
  }
}

void passColumns(const QImage& in, QImage& out, bool erode, int r) {
  WindowExtreme w(erode, in.height(), std::min(r, in.height()));
  for (int x = 0; x < in.width(); ++x) {
    for (int c = 0; c < 4; ++c) w.run(in.constBits() + x * 4 + c, in.bytesPerLine(), out.bits() + x * 4 + c, out.bytesPerLine());
  }
}

}  // namespace

// Filter Effects Level 1: a negative radius, or two zero radii, disable the
// primitive (the result is the input image). One zero radius leaves that axis
// alone. The window is 2r+1 pixels with r rounded up from the device radius.
Layer applyMorphology(const Env& env, const Morphology& p, ColorSpace, const Layer& in) {
  const double rx = env.userLength(p.radiusX, Axis::X), ry = env.userLength(p.radiusY, Axis::Y);
  if (!std::isfinite(rx) || !std::isfinite(ry) || rx < 0 || ry < 0 || (rx == 0 && ry == 0)) return in;
  const QPointF dev = env.extentToDevice(rx, ry);
  const int px = rx == 0 ? 0 : static_cast<int>(std::min(1.0e6, std::ceil(dev.x())));
  const int py = ry == 0 ? 0 : static_cast<int>(std::min(1.0e6, std::ceil(dev.y())));
  if (px == 0 && py == 0) return in;
  const bool erode = p.op != QLatin1String("dilate");
  QImage a(in.image.size(), QImage::Format_ARGB32_Premultiplied);
  QImage b = in.image;
  if (px > 0) {
    passRows(b, a, erode, px);
    b = a.copy();
  }
  if (py > 0) {
    passColumns(b, a, erode, py);
    b = a;
  }
  return {b, {}, in.space};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
