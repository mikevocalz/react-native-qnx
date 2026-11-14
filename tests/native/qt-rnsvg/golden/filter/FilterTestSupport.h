#pragma once

// Shared pieces of RnsvgFilterTest: a fixture that owns the primitives of one
// filter, the element renderings the scene would hand the engine, and the
// image comparison used against the resvg goldens.

#include "filter/FilterEngine.h"

#include <QImage>
#include <QPainter>
#include <QString>
#include <deque>

namespace filtertest {

using namespace rnqnx::views::rnsvg;
using namespace rnqnx::views::rnsvg::filter;

inline Length px(double v) { return Length{v, Length::Unit::Px, true}; }
inline Length pct(double v) { return Length{v, Length::Unit::Percent, true}; }
inline Brush colorBrush(const QColor& c) { return Brush{Brush::Type::Color, c, {}}; }

// The three element renderings the goldens use. All are opaque shapes on a
// transparent background in a 200x200 user space.
enum class Source { Red, TwoShapes, Bump };

inline QRectF boundingBox(Source s) {
  switch (s) {
    case Source::Red: return QRectF(40, 40, 120, 120);
    case Source::TwoShapes: return QRectF(40, 40, 125, 125);
    case Source::Bump: return QRectF(50, 50, 100, 100);
  }
  return {};
}

inline void paintSource(QPainter& p, Source s) {
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  switch (s) {
    case Source::Red:
      p.setBrush(QColor(0xe0, 0x30, 0x20));
      p.drawRect(QRectF(40, 40, 120, 120));
      break;
    case Source::TwoShapes:
      p.setBrush(QColor(0xe0, 0x30, 0x20));
      p.drawRect(QRectF(40, 40, 90, 90));
      p.setBrush(QColor(0x20, 0x60, 0xd0));
      p.drawEllipse(QPointF(120, 120), 45, 45);
      break;
    case Source::Bump:
      p.setBrush(QColor(0x88, 0x88, 0x88));
      p.drawEllipse(QPointF(100, 100), 50, 50);
      break;
  }
}

// One filter under test. Primitives live in a deque so the pointers in the
// Definition stay valid as more are added.
struct Fixture {
  Definition def;
  Context ctx;
  Source source{Source::Red};
  std::deque<Primitive> store;

  explicit Fixture(Source s = Source::Red) : source(s) {
    ctx.objectBoundingBox = boundingBox(s);
    ctx.viewport = QSizeF(200, 200);
  }

  Primitive& add(Params params) {
    store.emplace_back();
    Primitive& p = store.back();
    p.params = std::move(params);
    def.primitives.push_back(&p);
    return p;
  }

  // SVG's default filter region is -10%..120% of the box; goldens that need
  // the box exactly say so.
  void regionIsBox() {
    def.region.x = pct(0);
    def.region.y = pct(0);
    def.region.width = pct(100);
    def.region.height = pct(100);
  }
};

// The element drawn over the filter region, as the scene would supply it.
inline QImage renderSource(const Fixture& f, const QRect& region) {
  QImage image(region.size(), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.translate(-region.topLeft());
  painter.setTransform(f.ctx.userToDevice * painter.transform());
  paintSource(painter, f.source);
  return image;
}

// Runs the engine and composites the result over white on a canvas.
inline QImage runOnWhite(const Fixture& f, QSize canvas = QSize(200, 200)) {
  QImage out(canvas, QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::white);
  const QRect region = regionInDevice(f.def, f.ctx);
  if (region.isEmpty()) return out;
  const Result r = apply(f.def, f.ctx, renderSource(f, region));
  if (!r.valid) return out;
  QPainter painter(&out);
  painter.drawImage(r.rect.topLeft(), r.image);
  return out;
}

struct Diff {
  int maxDiff{0};        // largest per-channel difference
  double mean{0};        // mean absolute per-channel difference
  double overFraction{0};  // fraction of pixels with a channel off by more than `threshold`
};

inline Diff compare(const QImage& a, const QImage& b, int threshold) {
  Diff d;
  if (a.size() != b.size()) {
    d.maxDiff = 255;
    d.mean = 255;
    d.overFraction = 1;
    return d;
  }
  const QImage ca = a.convertToFormat(QImage::Format_ARGB32), cb = b.convertToFormat(QImage::Format_ARGB32);
  qint64 total = 0, over = 0;
  for (int y = 0; y < ca.height(); ++y) {
    const auto* pa = reinterpret_cast<const QRgb*>(ca.constScanLine(y));
    const auto* pb = reinterpret_cast<const QRgb*>(cb.constScanLine(y));
    for (int x = 0; x < ca.width(); ++x) {
      const int dr = std::abs(qRed(pa[x]) - qRed(pb[x])), dg = std::abs(qGreen(pa[x]) - qGreen(pb[x])),
                db = std::abs(qBlue(pa[x]) - qBlue(pb[x]));
      const int m = std::max({dr, dg, db});
      d.maxDiff = std::max(d.maxDiff, m);
      total += dr + dg + db;
      if (m > threshold) ++over;
    }
  }
  const double px = static_cast<double>(ca.width()) * ca.height();
  d.mean = static_cast<double>(total) / (px * 3.0);
  d.overFraction = static_cast<double>(over) / px;
  return d;
}

}  // namespace filtertest
