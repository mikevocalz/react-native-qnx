#pragma once

// Integer helpers for premultiplied 8-bit pixels.

#include <QRgb>

namespace rnqnx::views::rnsvg::filter::detail {

inline int div255(int x) { return (x + 127) / 255; }
inline int premul(int c, int a) { return div255(c * a); }
inline int demul(int c, int a) { return a == 0 ? 0 : (a == 255 ? c : (c * 255 + a / 2) / a); }

// `src` over `dst`, both premultiplied.
inline QRgb over(QRgb src, QRgb dst) {
  const int sa = qAlpha(src);
  if (sa == 255) return src;
  if (sa == 0) return dst;
  const int k = 255 - sa;
  return qRgba(qRed(src) + div255(qRed(dst) * k), qGreen(src) + div255(qGreen(dst) * k),
               qBlue(src) + div255(qBlue(dst) * k), sa + div255(qAlpha(dst) * k));
}

inline int clampByte(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

}  // namespace rnqnx::views::rnsvg::filter::detail
