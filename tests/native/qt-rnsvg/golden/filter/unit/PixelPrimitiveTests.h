#pragma once

// Per-pixel primitives checked on synthetic pixels: blur, color matrix, component transfer, composite, blend, merge, flood, morphology, drop shadow.
// Each function is one test; RnsvgFilterTest.cpp runs it as a slot. The QtTest
// macros work in any void function, so these are plain functions.

#include "../UnitSupport.h"

#include <QDir>
#include <limits>

namespace filtertest {

inline void gaussianBlurEdgeModesAndAxes() {
  // Opaque at both ends of a 20 pixel row, transparent in between.
  QImage src(20, 1, QImage::Format_ARGB32_Premultiplied);
  src.fill(Qt::transparent);
  for (int x : {0, 1, 2, 3, 16, 17, 18, 19}) reinterpret_cast<QRgb*>(src.scanLine(0))[x] = qRgba(255, 255, 255, 255);
  auto edgeAlpha = [&](EdgeMode mode) {
    Fixture f;
    exactRegion(f, 20, 1);
    GaussianBlur b;
    b.stdDeviationX = 1.5;
    b.stdDeviationY = 0;
    b.edgeMode = mode;
    f.add(b);
    return qAlpha(raw(run(f, src).image, 0, 0));
  };
  const int none = edgeAlpha(EdgeMode::None), dup = edgeAlpha(EdgeMode::Duplicate), wrap = edgeAlpha(EdgeMode::Wrap);
  QVERIFY2(none >= 140 && none <= 185, qPrintable(QString::number(none)));  // zeros outside pull it down
  QVERIFY2(dup >= 240, qPrintable(QString::number(dup)));
  QVERIFY2(wrap >= 240, qPrintable(QString::number(wrap)));  // the far end wraps in
  // One zero deviation blurs along the other axis only: a horizontal edge stays sharp
  // under a horizontal blur.
  Fixture f;
  exactRegion(f, 30, 30);
  detail::blur(f, 5, 0);
  const Result r = run(f, withRect(30, 30, QRect(0, 10, 30, 10), Qt::white));
  VERIFY_RAW(r, 15, 9, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 15, 10, 255, 255, 255, 255, 0);
  // Blur keeps premultiplied invariants and roughly conserves alpha mass.
  Fixture g;
  exactRegion(g, 60, 60);
  detail::blur(g, 6, 6);
  const QImage s = withRect(60, 60, QRect(20, 20, 20, 20), QColor(200, 100, 50));
  const Result b = run(g, s);
  QVERIFY(isPremultiplied(b.image));
  QVERIFY(std::abs(totalAlpha(b.image) / totalAlpha(s) - 1.0) < 0.01);
}

inline void boxBlurMatchesDirectComputation() {
  // The three box passes of the SVG specification written out naively on the
  // alpha channel of a single row, for every edge mode and for windows from
  // smaller than the row to many times larger.
  auto edgeIndex = [](int i, int n, EdgeMode m) {
    if (i >= 0 && i < n) return i;
    if (m == EdgeMode::None) return -1;
    if (m == EdgeMode::Duplicate) return i < 0 ? 0 : n - 1;
    return ((i % n) + n) % n;
  };
  auto pass = [&](const std::vector<int>& in, int lo, int hi, EdgeMode m) {
    const int n = static_cast<int>(in.size());
    const float inv = 1.0f / static_cast<float>(lo + hi + 1);
    std::vector<int> out(in.size());
    for (int i = 0; i < n; ++i) {
      long long sum = 0;
      for (int k = -lo; k <= hi; ++k) {
        const int j = edgeIndex(i + k, n, m);
        if (j >= 0) sum += in[static_cast<size_t>(j)];
      }
      out[static_cast<size_t>(i)] = static_cast<int>(std::min(255.0f, static_cast<float>(sum) * inv + 0.5f));
    }
    return out;
  };
  for (EdgeMode mode : {EdgeMode::None, EdgeMode::Duplicate, EdgeMode::Wrap}) {
    for (int width : {1, 2, 5, 9, 13}) {
      for (double sigma : {2.0, 2.4, 3.0, 5.5, 20.0}) {
        QImage src(width, 1, QImage::Format_ARGB32_Premultiplied);
        std::vector<int> row(static_cast<size_t>(width));
        for (int x = 0; x < width; ++x) {
          row[static_cast<size_t>(x)] = (x * 71 + 40) % 256;
          reinterpret_cast<QRgb*>(src.scanLine(0))[x] = qRgba(0, 0, 0, row[static_cast<size_t>(x)]);
        }
        const int d = static_cast<int>(std::floor(sigma * 3.0 * std::sqrt(2.0 * 3.14159265358979323846) / 4.0 + 0.5));
        std::vector<int> expected = row;
        if (d % 2 == 1) {
          for (int i = 0; i < 3; ++i) expected = pass(expected, (d - 1) / 2, (d - 1) / 2, mode);
        } else {
          expected = pass(expected, d / 2, d / 2 - 1, mode);
          expected = pass(expected, d / 2 - 1, d / 2, mode);
          expected = pass(expected, d / 2, d / 2, mode);
        }
        Fixture f;
        exactRegion(f, width, 1);
        GaussianBlur b;
        b.stdDeviationX = sigma;
        b.stdDeviationY = 0;
        b.edgeMode = mode;
        f.add(b).colorSpace = ColorSpace::SRGB;
        const Result r = run(f, src);
        for (int x = 0; x < width; ++x) {
          QVERIFY2(qAlpha(raw(r.image, x, 0)) == expected[static_cast<size_t>(x)],
                   qPrintable(QStringLiteral("mode %1 width %2 sigma %3 x %4: got %5 expected %6")
                                  .arg(static_cast<int>(mode)).arg(width).arg(sigma).arg(x).arg(qAlpha(raw(r.image, x, 0)))
                                  .arg(expected[static_cast<size_t>(x)])));
        }
      }
    }
  }
}

inline void colorMatrixValues() {
  auto matrixAt = [&](const char* type, std::vector<double> values, QColor in) {
    Fixture f;
    exactRegion(f, 4, 4);
    detail::colorMatrix(f, type, std::move(values));
    f.store.back().colorSpace = ColorSpace::SRGB;
    return raw(run(f, solid(4, 4, in)).image, 1, 1);
  };
  EXPECT_CLOSE(matrixAt("saturate", {0}, QColor(255, 0, 0)), 54, 54, 54, 255, 1);  // 0.213 * 255
  EXPECT_CLOSE(matrixAt("saturate", {1}, QColor(200, 100, 50)), 200, 100, 50, 255, 0);
  EXPECT_CLOSE(matrixAt("hueRotate", {0}, QColor(200, 100, 50)), 200, 100, 50, 255, 0);
  EXPECT_CLOSE(matrixAt("hueRotate", {180}, QColor(255, 255, 255)), 255, 255, 255, 255, 1);
  EXPECT_CLOSE(matrixAt("luminanceToAlpha", {}, QColor(255, 255, 255)), 0, 0, 0, 255, 1);
  EXPECT_CLOSE(matrixAt("luminanceToAlpha", {}, QColor(0, 0, 0)), 0, 0, 0, 0, 0);
  // matrix: channel swap, and a bias row. 20 values; other counts mean identity.
  EXPECT_CLOSE(matrixAt("matrix", {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0}, QColor(10, 20, 30)), 20, 30, 10, 255, 1);
  EXPECT_CLOSE(matrixAt("matrix", {1, 0, 0, 0, 0.5, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0}, QColor(0, 0, 0)), 128, 0, 0, 255, 1);
  EXPECT_CLOSE(matrixAt("matrix", {0, 0, 0}, QColor(10, 20, 30)), 10, 20, 30, 255, 0);
  EXPECT_CLOSE(matrixAt("saturate", {}, QColor(10, 20, 30)), 10, 20, 30, 255, 0);
  EXPECT_CLOSE(matrixAt("nonsense", {1}, QColor(10, 20, 30)), 10, 20, 30, 255, 0);
}

inline void componentTransferFunctions() {
  auto apply1 = [&](TransferFunction tf, int channel, QColor in) {
    Fixture f;
    exactRegion(f, 4, 4);
    TransferFunction id = detail::fn("identity");
    detail::componentTransfer(f, channel == 0 ? tf : id, channel == 1 ? tf : id, channel == 2 ? tf : id, channel == 3 ? tf : id);
    f.store.back().colorSpace = ColorSpace::SRGB;
    return raw(run(f, solid(4, 4, in)).image, 1, 1);
  };
  const QColor in(200, 100, 50);
  EXPECT_CLOSE(apply1(detail::fn("table", {1, 0}), 0, in), 55, 100, 50, 255, 1);                 // inverts
  EXPECT_CLOSE(apply1(detail::fn("table", {0, 1, 0}), 0, QColor(128, 0, 0)), 255, 0, 0, 255, 2);  // peak at the middle
  EXPECT_CLOSE(apply1(detail::fn("table", {0.5}), 1, in), 200, 128, 50, 255, 1);                    // one value: constant
  EXPECT_CLOSE(apply1(detail::fn("discrete", {0, 1}), 0, in), 255, 100, 50, 255, 0);               // 200/255 >= 0.5
  EXPECT_CLOSE(apply1(detail::fn("discrete", {0, 1}), 1, in), 200, 0, 50, 255, 0);
  EXPECT_CLOSE(apply1(detail::fn("discrete", {0.25, 0.5, 0.75, 1}), 2, in), 200, 100, 64, 255, 1);
  EXPECT_CLOSE(apply1(detail::fn("linear", {}, 2, -0.5), 1, in), 200, 73, 50, 255, 1);            // 2*.392-.5 = .284
  EXPECT_CLOSE(apply1(detail::fn("gamma", {}, 1, 0, 1, 2, 0), 0, in), 157, 100, 50, 255, 1);      // .784^2
  EXPECT_CLOSE(apply1(detail::fn("gamma", {}, 1, 0, 2, 1, 0.5), 0, in), 255, 100, 50, 255, 0);    // clamped
  EXPECT_CLOSE(apply1(detail::fn("table", {}), 0, in), 200, 100, 50, 255, 0);                      // empty table: identity
  EXPECT_CLOSE(apply1(detail::fn("discrete", {}), 0, in), 200, 100, 50, 255, 0);
  EXPECT_CLOSE(apply1(detail::fn("bogus"), 0, in), 200, 100, 50, 255, 0);
  // Alpha: the other channels are demultiplied first, so halving alpha halves the premultiplied pixel.
  const QRgb half = apply1(detail::fn("linear", {}, 0.5), 3, QColor(200, 100, 50));
  QVERIFY2(close(half, 100, 50, 25, 128, 1), qPrintable(describe(half)));
}

inline void compositeOperators() {
  ColorSpace space = ColorSpace::SRGB;
  auto pixel = [&](const char* op, const char* a, const char* b, double k1 = 0, double k2 = 0, double k3 = 0, double k4 = 0) {
    Fixture f;
    exactRegion(f, 4, 4);
    detail::flood(f, a, 1.0, "a");
    detail::flood(f, b, 1.0, "b");
    Composite c;
    c.in2 = QStringLiteral("b");
    c.op = QLatin1String(op);
    c.k1 = k1; c.k2 = k2; c.k3 = k3; c.k4 = k4;
    Primitive& p = f.add(c);
    p.in1 = QStringLiteral("a");
    p.colorSpace = space;
    return raw(run(f, QImage()).image, 1, 1);
  };
  EXPECT_CLOSE(pixel("over", "#ff0000", "#0000ff"), 255, 0, 0, 255, 0);
  EXPECT_CLOSE(pixel("in", "#ff0000", "#0000ff"), 255, 0, 0, 255, 0);
  QCOMPARE(qAlpha(pixel("out", "#ff0000", "#0000ff")), 0);
  EXPECT_CLOSE(pixel("atop", "#ff0000", "#0000ff"), 255, 0, 0, 255, 0);
  QCOMPARE(qAlpha(pixel("xor", "#ff0000", "#0000ff")), 0);
  EXPECT_CLOSE(pixel("arithmetic", "#ff0000", "#0000ff", 0, 0.5, 0.5, 0), 128, 0, 128, 255, 1);
  EXPECT_CLOSE(pixel("arithmetic", "#ff0000", "#ff0000", 1, 0, 0, 0), 255, 0, 0, 255, 0);   // k1 multiplies
  EXPECT_CLOSE(pixel("arithmetic", "#ff0000", "#0000ff", 0, 0, 0, 0.5), 128, 128, 128, 128, 1);  // k4 alone: alpha .5, colors capped by it
  EXPECT_CLOSE(pixel("arithmetic", "#ff0000", "#0000ff", 0, 1, 1, 0), 255, 0, 255, 255, 0);
  EXPECT_CLOSE(pixel("arithmetic", "#ff0000", "#0000ff", 0, 0, -1, 0), 0, 0, 0, 0, 0);      // negatives clamp to 0
  EXPECT_CLOSE(pixel("unknown", "#ff0000", "#0000ff"), 255, 0, 0, 255, 0);                    // treated as over
  space = ColorSpace::LinearRGB;  // the same average taken on light: 0.5 linear is 188 in sRGB
  EXPECT_CLOSE(pixel("arithmetic", "#ff0000", "#0000ff", 0, 0.5, 0.5, 0), 188, 0, 188, 255, 1);
  space = ColorSpace::SRGB;
  // Half-alpha inputs through the Porter-Duff rules.
  Fixture f;
  exactRegion(f, 4, 4);
  detail::flood(f, "#ff0000", 0.5, "a");
  detail::flood(f, "#0000ff", 0.5, "b");
  Composite c;
  c.in2 = QStringLiteral("b");
  c.op = QStringLiteral("over");
  f.add(c).in1 = QStringLiteral("a");
  f.store.back().colorSpace = ColorSpace::SRGB;
  const QRgb over = raw(run(f, QImage()).image, 1, 1);
  QVERIFY2(close(over, 128, 0, 64, 192, 1), qPrintable(describe(over)));  // a over b: alpha .5+.5*.5
}

inline void blendModes() {
  auto pixel = [&](const char* mode, const char* top, double topAlpha, const char* back) {
    Fixture f;
    exactRegion(f, 4, 4);
    detail::flood(f, top, topAlpha, "a");
    detail::flood(f, back, 1.0, "b");
    Blend b;
    b.in2 = QStringLiteral("b");
    b.mode = QLatin1String(mode);
    Primitive& p = f.add(b);
    p.in1 = QStringLiteral("a");
    p.colorSpace = ColorSpace::SRGB;
    return raw(run(f, QImage()).image, 1, 1);
  };
  EXPECT_CLOSE(pixel("normal", "#ff0000", 0.5, "#0000ff"), 128, 0, 127, 255, 1);
  EXPECT_CLOSE(pixel("multiply", "#ff0000", 1.0, "#ffff00"), 255, 0, 0, 255, 1);
  EXPECT_CLOSE(pixel("screen", "#ff0000", 1.0, "#0000ff"), 255, 0, 255, 255, 1);
  EXPECT_CLOSE(pixel("darken", "#ff8000", 1.0, "#80ff00"), 128, 128, 0, 255, 1);
  EXPECT_CLOSE(pixel("lighten", "#ff8000", 1.0, "#80ff00"), 255, 255, 0, 255, 1);
  EXPECT_CLOSE(pixel("difference", "#ff0000", 1.0, "#ffff00"), 0, 255, 0, 255, 1);
  EXPECT_CLOSE(pixel("exclusion", "#ffffff", 1.0, "#000000"), 255, 255, 255, 255, 1);
  EXPECT_CLOSE(pixel("color", "#808080", 1.0, "#ff0000"), 255, 0, 0, 255, 255);  // some result; see goldens for values
  EXPECT_CLOSE(pixel("luminosity", "#000000", 1.0, "#ff0000"), 0, 0, 0, 255, 1);  // black luminosity over red
  EXPECT_CLOSE(pixel("bogus", "#ff0000", 1.0, "#0000ff"), 255, 0, 0, 255, 0);      // normal
}

inline void mergeAndFloodArithmetic() {
  Fixture f;
  exactRegion(f, 4, 4);
  flood(f, "#ff0000", 1.0, "a");
  flood(f, "#0000ff", 0.5, "b");
  detail::merge(f, {QStringLiteral("a"), QStringLiteral("b")});  // later nodes are painted over earlier ones
  f.store.back().colorSpace = ColorSpace::SRGB;
  VERIFY_RAW(run(f, QImage()), 1, 1, 127, 0, 128, 255, 1);
  f.store.clear();
  f.def.primitives.clear();
  flood(f, "#ff0000", 0.5);
  VERIFY_RAW(run(f, QImage()), 1, 1, 128, 0, 0, 128, 1);
  f.store.clear();
  f.def.primitives.clear();
  Flood cur;
  cur.color = Brush{Brush::Type::CurrentColor, {}, {}};
  f.ctx.currentColor = QColor(0, 255, 0);
  f.add(cur);
  VERIFY_RAW(run(f, QImage()), 1, 1, 0, 255, 0, 255, 0);
  f.store.clear();
  f.def.primitives.clear();
  Flood none;  // a url() reference or `none` has no color to flood with
  none.color = Brush{Brush::Type::Ref, {}, QStringLiteral("p")};
  f.add(none);
  QCOMPARE(totalAlpha(run(f, QImage()).image), 0.0);
  f.store.clear();
  f.def.primitives.clear();
  Flood semi;  // the alpha of the color multiplies flood-opacity
  semi.color = detail::c("#ff0000");
  semi.color.color.setAlphaF(0.5);
  semi.opacity = 0.5;
  f.add(semi);
  VERIFY_RAW(run(f, QImage()), 1, 1, 64, 0, 0, 64, 1);
}

inline void morphologyRadii() {
  const QImage block = withRect(21, 21, QRect(5, 5, 11, 11), Qt::white);
  auto run1 = [&](const char* op, double rx, double ry, const QImage& src) {
    Fixture f;
    exactRegion(f, src.width(), src.height());
    detail::morph(f, op, rx, ry);
    return run(f, src);
  };
  Result r = run1("erode", 2, 2, block);  // a 2 pixel border goes
  VERIFY_RAW(r, 7, 7, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 6, 10, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 13, 13, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 14, 10, 0, 0, 0, 0, 0);
  r = run1("dilate", 3, 3, block);
  VERIFY_RAW(r, 2, 10, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 1, 10, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 18, 18, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 19, 10, 0, 0, 0, 0, 0);
  r = run1("erode", 3, 0, block);  // one zero radius leaves that axis alone
  VERIFY_RAW(r, 8, 5, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 7, 5, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 10, 4, 0, 0, 0, 0, 0);
  // The region edge counts as transparent: an opaque region erodes inward from its border.
  r = run1("erode", 2, 2, solid(10, 10, Qt::white));
  VERIFY_RAW(r, 1, 5, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 2, 5, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 7, 5, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 8, 5, 0, 0, 0, 0, 0);
  // Radius 0 and negative radii disable the primitive: the input comes through.
  for (double bad : {0.0, -1.0}) {
    r = run1("erode", bad, bad, block);
    VERIFY_RAW(r, 5, 5, 255, 255, 255, 255, 1);
    VERIFY_RAW(r, 4, 5, 0, 0, 0, 0, 1);
  }
  // A radius larger than the image: erode empties it, dilate fills it.
  QCOMPARE(totalAlpha(run1("erode", 1000, 1000, block).image), 0.0);
  QCOMPARE(totalAlpha(run1("dilate", 1000, 1000, block).image), 21.0 * 21.0 * 255.0);
}

inline void dropShadowArithmetic() {
  Fixture f;
  exactRegion(f, 40, 40);
  DropShadow d;
  d.dx = px(10);
  d.dy = px(10);
  d.stdDeviationX = d.stdDeviationY = 0;
  d.color = detail::c("#000000");
  d.opacity = 1;
  f.add(d).colorSpace = ColorSpace::SRGB;
  const Result r = run(f, withRect(40, 40, QRect(5, 5, 10, 10), Qt::white));
  VERIFY_RAW(r, 8, 8, 255, 255, 255, 255, 0);      // the element on top
  VERIFY_RAW(r, 20, 20, 0, 0, 0, 255, 0);          // the shadow, offset by (10,10)
  VERIFY_RAW(r, 14, 14, 255, 255, 255, 255, 0);    // element still over the shadow
  VERIFY_RAW(r, 30, 30, 0, 0, 0, 0, 0);
  // Blurred: the shadow darkens a ring around it and keeps its mass.
  Fixture g;
  exactRegion(g, 60, 60);
  DropShadow b = d;
  b.stdDeviationX = b.stdDeviationY = 3;
  b.opacity = 0.5;
  g.add(b).colorSpace = ColorSpace::SRGB;
  const QImage src = withRect(60, 60, QRect(10, 10, 10, 10), Qt::white);
  const Result rb = run(g, src);
  QVERIFY(isPremultiplied(rb.image));
  QVERIFY(std::abs(totalAlpha(rb.image) - (totalAlpha(src) + 0.5 * totalAlpha(src))) < 0.02 * totalAlpha(src));
}

}  // namespace filtertest
