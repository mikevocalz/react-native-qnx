#pragma once

// Helpers for the oracle-free unit tests in RnsvgFilterTest: exact-size
// fixtures (the filter region equals the source image, one user unit per
// pixel), synthetic sources and raw premultiplied pixel checks.

#include "GoldenCases.h"

#include <QElapsedTimer>
#include <QTest>
#include <cmath>
#include <limits>

namespace filtertest {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

inline Primitive& flood(Fixture& f, const char* hex, double opacity = 1.0, const char* result = "") {
  detail::flood(f, hex, opacity, result);
  return f.store.back();
}

// feOffset by 0 copies an input, which is the cheapest way to look at one.
inline Primitive& passThrough(Fixture& f, const char* in) {
  detail::offset(f, 0, 0, in);
  return f.store.back();
}

// A filter region of exactly w x h device pixels at the origin.
inline void exactRegion(Fixture& f, int w, int h) {
  f.def.region.filterUserSpace = true;
  f.def.region.x = px(0);
  f.def.region.y = px(0);
  f.def.region.width = px(w);
  f.def.region.height = px(h);
  f.ctx.objectBoundingBox = QRectF(0, 0, w, h);
  f.ctx.viewport = QSizeF(w, h);
}

inline QImage solid(int w, int h, const QColor& c) {
  QImage im(w, h, QImage::Format_ARGB32_Premultiplied);
  im.fill(c);
  return im;
}

inline QImage withRect(int w, int h, const QRect& r, const QColor& c) {
  QImage im(w, h, QImage::Format_ARGB32_Premultiplied);
  im.fill(Qt::transparent);
  QPainter p(&im);
  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(r, c);
  return im;
}

inline Result run(const Fixture& f, const QImage& source) { return apply(f.def, f.ctx, source); }

// The premultiplied pixel as stored.
inline QRgb raw(const QImage& im, int x, int y) {
  return reinterpret_cast<const QRgb*>(im.constScanLine(y))[x];
}

inline bool close(QRgb p, int r, int g, int b, int a, int tol) {
  return std::abs(qRed(p) - r) <= tol && std::abs(qGreen(p) - g) <= tol && std::abs(qBlue(p) - b) <= tol &&
         std::abs(qAlpha(p) - a) <= tol;
}

inline QString describe(QRgb p) {
  return QStringLiteral("(%1,%2,%3,a=%4)").arg(qRed(p)).arg(qGreen(p)).arg(qBlue(p)).arg(qAlpha(p));
}

// Every channel at most the alpha, which premultiplied storage requires.
inline bool isPremultiplied(const QImage& im) {
  if (im.format() != QImage::Format_ARGB32_Premultiplied) return false;
  for (int y = 0; y < im.height(); ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(im.constScanLine(y));
    for (int x = 0; x < im.width(); ++x) {
      if (qRed(s[x]) > qAlpha(s[x]) || qGreen(s[x]) > qAlpha(s[x]) || qBlue(s[x]) > qAlpha(s[x])) return false;
    }
  }
  return true;
}

inline bool sameBytes(const QImage& a, const QImage& b) {
  if (a.size() != b.size() || a.format() != b.format()) return false;
  for (int y = 0; y < a.height(); ++y) {
    if (std::memcmp(a.constScanLine(y), b.constScanLine(y), static_cast<size_t>(a.bytesPerLine())) != 0) return false;
  }
  return true;
}

inline double totalAlpha(const QImage& im) {
  double t = 0;
  for (int y = 0; y < im.height(); ++y) {
    const auto* s = reinterpret_cast<const QRgb*>(im.constScanLine(y));
    for (int x = 0; x < im.width(); ++x) t += qAlpha(s[x]);
  }
  return t;
}

}  // namespace filtertest

// A pixel check that says what the pixel was.
#define EXPECT_CLOSE(pixel, r, g, b, a, tol)                                                    \
  do {                                                                                          \
    const QRgb v_ = (pixel);                                                                    \
    QVERIFY2(filtertest::close(v_, (r), (g), (b), (a), (tol)),                                  \
             qPrintable(QStringLiteral("%1, expected (%2,%3,%4,a=%5) +-%6")                    \
                            .arg(filtertest::describe(v_)).arg(r).arg(g).arg(b).arg(a).arg(tol))); \
  } while (0)

// QVERIFY2 with a pixel diagnostic. `res` is a Result.
#define VERIFY_RAW(res, x, y, r, g, b, a, tol)                                                          \
  do {                                                                                                  \
    const QRgb got_ = filtertest::raw((res).image, (x), (y));                                           \
    QVERIFY2(filtertest::close(got_, (r), (g), (b), (a), (tol)),                                        \
             qPrintable(QStringLiteral("pixel (%1,%2) is %3, expected (%4,%5,%6,a=%7) +-%8")           \
                            .arg(x).arg(y).arg(filtertest::describe(got_)).arg(r).arg(g).arg(b).arg(a) \
                            .arg(tol)));                                                                \
  } while (0)
