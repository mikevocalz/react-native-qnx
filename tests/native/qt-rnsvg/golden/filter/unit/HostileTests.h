#pragma once

// Invalid and hostile input, blur cost, and that apply() never writes to its source.
// Each function is one test; RnsvgFilterTest.cpp runs it as a slot. The QtTest
// macros work in any void function, so these are plain functions.

#include "../UnitSupport.h"

#include <QDir>
#include <limits>

namespace filtertest {

inline void invalidParametersFollowTheirRules() {
  const QImage src = withRect(20, 20, QRect(5, 5, 10, 10), QColor(200, 100, 50));
  auto sameAsInput = [&](const std::function<void(Fixture&)>& build, const char* what) {
    Fixture f;
    exactRegion(f, 20, 20);
    build(f);
    f.store.back().colorSpace = ColorSpace::SRGB;
    const Result r = run(f, src);
    QVERIFY2(r.valid && sameBytes(r.image, src), what);
  };
  auto transparent = [&](const std::function<void(Fixture&)>& build, const char* what) {
    Fixture f;
    exactRegion(f, 20, 20);
    build(f);
    const Result r = run(f, src);
    QVERIFY2(r.valid && totalAlpha(r.image) == 0.0, what);
  };
  // Disabled primitives return their input.
  sameAsInput([](Fixture& f) { detail::blur(f, -1, 2); }, "negative blur");
  sameAsInput([](Fixture& f) { detail::blur(f, 0, 0); }, "zero blur");
  sameAsInput([](Fixture& f) { detail::blur(f, kNaN, 1); }, "NaN blur");
  sameAsInput([](Fixture& f) { detail::blur(f, kInf, 1); }, "infinite blur");
  sameAsInput([](Fixture& f) { detail::morph(f, "erode", -1, 1); }, "negative radius");
  sameAsInput([](Fixture& f) { detail::morph(f, "dilate", 0, 0); }, "zero radius");
  sameAsInput([](Fixture& f) { detail::colorMatrix(f, "matrix", {1, 2, 3}); }, "short matrix");
  sameAsInput([](Fixture& f) {
    detail::componentTransfer(f, detail::fn("table", {}), detail::fn("discrete", {}), detail::fn("bogus"), detail::fn("identity"));
  }, "empty tables");
  // Primitives with no input to fall back on are transparent.
  transparent([](Fixture& f) { detail::convolve(f, 3, 3, {1, 2}, 1, EdgeMode::None); }, "kernel too short");
  transparent([](Fixture& f) { detail::convolve(f, 0, 3, {}, 1, EdgeMode::None); }, "order 0");
  transparent([](Fixture& f) { detail::convolve(f, -3, 1, {1, 1, 1}, 1, EdgeMode::None); }, "negative order");
  transparent([](Fixture& f) { detail::convolve(f, 3, 1, {1, 1, 1}, 1, EdgeMode::None, false, 3, 0); }, "target out of range");
  transparent([](Fixture& f) { detail::convolve(f, 3, 1, {1, 1, 1}, kNaN, EdgeMode::None); }, "NaN divisor");
  transparent([](Fixture& f) { detail::turbulence(f, "turbulence", -1, 0.1, 1, 1); }, "negative frequency");
  transparent([](Fixture& f) {
    SpecularLighting s;
    s.specularExponent = 0.5;
    s.light = detail::distant();
    f.add(s);
  }, "specularExponent below 1");
  transparent([](Fixture& f) {
    SpecularLighting s;
    s.specularExponent = 129;
    s.light = detail::distant();
    f.add(s);
  }, "specularExponent above 128");
  transparent([](Fixture& f) {
    DiffuseLighting d;
    d.diffuseConstant = -1;
    d.light = detail::distant();
    f.add(d);
  }, "negative diffuseConstant");
  // feImage without any way to load.
  transparent([](Fixture& f) { f.add(Image{QStringLiteral("a.png"), QStringLiteral("xMidYMid"), 0}); }, "no loader");
  // A negative drop shadow deviation is 0, so the shadow is sharp and still drawn.
  Fixture f;
  exactRegion(f, 20, 20);
  DropShadow d;
  d.dx = px(2);
  d.dy = px(0);
  d.stdDeviationX = -4;
  d.stdDeviationY = 1;
  d.color = detail::c("#000000");
  f.add(d).colorSpace = ColorSpace::SRGB;
  VERIFY_RAW(run(f, src), 16, 10, 0, 0, 0, 255, 0);  // right edge of the shadow, 2 pixels past the element
}

inline void hostileParametersStayBounded() {
  struct Case {
    const char* name;
    std::function<void(Fixture&)> build;
  };
  const std::vector<Case> cases = {
      {"blur 1e300", [](Fixture& f) { detail::blur(f, 1e300, 1e300); }},
      {"blur 1e9 wrap", [](Fixture& f) {
         GaussianBlur b;
         b.stdDeviationX = b.stdDeviationY = 1e9;
         b.edgeMode = EdgeMode::Wrap;
         f.add(b);
       }},
      {"blur huge duplicate", [](Fixture& f) {
         GaussianBlur b;
         b.stdDeviationX = 3e6;
         b.stdDeviationY = 2;
         b.edgeMode = EdgeMode::Duplicate;
         f.add(b);
       }},
      {"offset 1e300", [](Fixture& f) { detail::offset(f, 1e300, -1e300); }},
      {"offset NaN", [](Fixture& f) { detail::offset(f, kNaN, kInf); }},
      {"morphology 1e9", [](Fixture& f) { detail::morph(f, "dilate", 1e9, 1e9); }},
      {"morphology inf", [](Fixture& f) { detail::morph(f, "erode", kInf, 1); }},
      {"turbulence octaves 2^30", [](Fixture& f) { detail::turbulence(f, "turbulence", 0.1, 0.1, 1 << 30, 1); }},
      {"turbulence INT_MIN octaves", [](Fixture& f) { detail::turbulence(f, "fractalNoise", 0.1, 0.1, std::numeric_limits<int>::min(), 1); }},
      {"turbulence seed NaN", [](Fixture& f) { detail::turbulence(f, "turbulence", 0.1, 0.1, 2, kNaN); }},
      {"turbulence seed 1e300", [](Fixture& f) { detail::turbulence(f, "turbulence", 0.1, 0.1, 2, 1e300); }},
      {"turbulence freq 1e300", [](Fixture& f) { detail::turbulence(f, "turbulence", 1e300, 1e300, 3, 1); }},
      {"turbulence freq inf stitched", [](Fixture& f) { detail::turbulence(f, "turbulence", kInf, 0.1, 3, 1, true); }},
      {"turbulence stitched, huge tile and frequency", [](Fixture& f) {
         detail::turbulence(f, "turbulence", 1e300, 1e-300, 24, 3, true);
         Primitive& p = f.store.back();
         p.subregion.x = px(1e7);
         p.subregion.width = px(1e7);
       }},
      {"turbulence stitched, 24 octaves, large tile", [](Fixture& f) {
         detail::turbulence(f, "fractalNoise", 1e5, 1e5, 24, 3, true);
         Primitive& p = f.store.back();
         p.subregion.width = px(1e6);
         p.subregion.height = px(1e6);
       }},
      {"displacement NaN", [](Fixture& f) { detail::turbulence(f, "turbulence", 0.1, 0.1, 1, 1); detail::displacement(f, kNaN, "R", "G"); }},
      {"displacement 1e300", [](Fixture& f) { detail::turbulence(f, "turbulence", 0.1, 0.1, 1, 1); detail::displacement(f, 1e300, "R", "G"); }},
      {"convolve order 1000", [](Fixture& f) {
         detail::convolve(f, 1000, 1000, std::vector<double>(1000 * 1000, 1.0), 1, EdgeMode::Wrap);
       }},
      {"convolve order INT_MAX", [](Fixture& f) { detail::convolve(f, std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), {1}, 1, EdgeMode::None); }},
      {"convolve at the order cap", [](Fixture& f) { detail::convolve(f, 32, 32, std::vector<double>(1024, 1.0), 0, EdgeMode::Wrap); }},
      {"convolve bias inf", [](Fixture& f) { detail::convolve(f, 1, 1, {1}, 1, EdgeMode::None); static_cast<void>(f); std::get<ConvolveMatrix>(f.store.back().params).bias = kInf; }},
      {"transfer NaN", [](Fixture& f) {
         detail::componentTransfer(f, detail::fn("table", {kNaN, kInf, -kInf}), detail::fn("gamma", {}, 1, 0, kNaN, kNaN, kNaN),
                                   detail::fn("linear", {}, kInf, kNaN), detail::fn("discrete", {kNaN}));
       }},
      {"color matrix NaN", [](Fixture& f) { detail::colorMatrix(f, "matrix", std::vector<double>(20, kNaN)); }},
      {"hue rotate inf", [](Fixture& f) { detail::colorMatrix(f, "hueRotate", {kInf}); }},
      {"saturate huge", [](Fixture& f) { detail::colorMatrix(f, "saturate", {1e300}); }},
      {"arithmetic NaN", [](Fixture& f) {
         detail::offset(f, 1, 1, "", "o");
         detail::composite(f, "arithmetic", kNaN, kInf, -kInf, 1e300);
       }},
      {"flood opacity NaN", [](Fixture& f) { detail::flood(f, "#ff0000", kNaN); }},
      {"flood opacity inf", [](Fixture& f) { detail::flood(f, "#ff0000", kInf); }},
      {"diffuse NaN", [](Fixture& f) {
         DiffuseLighting d;
         d.surfaceScale = kNaN;
         d.diffuseConstant = kNaN;
         d.light = detail::point();
         f.add(d);
       }},
      {"point light inf", [](Fixture& f) {
         DiffuseLighting d;
         d.surfaceScale = 1e300;
         d.light = detail::point();
         d.light.x = kInf;
         d.light.y = kNaN;
         d.light.z = -1e300;
         f.add(d);
       }},
      {"spot light NaN", [](Fixture& f) {
         SpecularLighting s;
         s.specularExponent = 2;
         s.light = detail::spot(kNaN, kNaN);
         s.light.pointsAtX = kInf;
         f.add(s);
       }},
      {"spot light negative exponent", [](Fixture& f) {
         DiffuseLighting d;
         d.light = detail::spot(-5, 1e9);
         f.add(d);
       }},
      {"drop shadow NaN", [](Fixture& f) {
         DropShadow d;
         d.dx = px(kNaN);
         d.dy = px(kInf);
         d.stdDeviationX = kNaN;
         d.stdDeviationY = 1e300;
         d.opacity = kNaN;
         f.add(d);
       }},
      {"image missing loaders", [](Fixture& f) { f.add(Image{QString(), QString(), 7}); }},
      {"image odd align", [](Fixture& f) {
         f.ctx.loadImage = [](const QString&) { return solid(3, 3, Qt::red); };
         f.add(Image{QStringLiteral("a"), QStringLiteral("garbage"), -1});
       }},
      {"image 1x1 slice into huge subregion", [](Fixture& f) {
         f.ctx.loadImage = [](const QString&) { return solid(1, 1, Qt::red); };
         Primitive& p = f.add(Image{QStringLiteral("a"), QStringLiteral("xMidYMid"), 1});
         p.subregion.width = p.subregion.height = px(1e7);
       }},
      {"tile of nothing", [](Fixture& f) {
         Primitive& o = f.add(Offset{});
         o.result = QStringLiteral("o");
         o.subregion.width = px(0);
         f.add(Tile{}).in1 = QStringLiteral("o");
       }},
      {"subregion at 1e300", [](Fixture& f) {
         Primitive& p = flood(f, "#ff0000");
         p.subregion.x = px(1e300);
         p.subregion.width = px(1e300);
       }},
      {"a thousand primitives", [](Fixture& f) {
         for (int i = 0; i < 1000; ++i) detail::offset(f, 1, 0, "", i % 7 == 0 ? "seven" : "");
       }},
      {"self and circular names", [](Fixture& f) {
         for (int i = 0; i < 20; ++i) detail::offset(f, 1, 0, "a", "a");
         detail::merge(f, {QStringLiteral("a"), QStringLiteral("zz"), QStringLiteral("a"), QString()});
       }},
  };
  for (const Case& c : cases) {
    Fixture f;
    exactRegion(f, 64, 48);
    c.build(f);
    QElapsedTimer timer;
    timer.start();
    const Result r = run(f, withRect(64, 48, QRect(10, 10, 30, 20), QColor(200, 100, 50)));
    const qint64 ms = timer.elapsed();
    QVERIFY2(r.valid, c.name);
    QVERIFY2(ms < 3000, qPrintable(QStringLiteral("%1 took %2 ms").arg(c.name).arg(ms)));
    QVERIFY2(r.image.size() == QSize(64, 48) && isPremultiplied(r.image), c.name);
  }
}

inline void hostileSourcesAndRegions() {
  Fixture f;
  exactRegion(f, 16, 16);
  detail::blur(f, 2, 2);
  // Null, wrong size, wrong format and non-premultiplied sources.
  QImage straight(16, 16, QImage::Format_ARGB32);
  straight.fill(QColor(255, 0, 0, 128));
  for (const QImage& source : {QImage(), solid(3, 3, Qt::red), solid(100, 100, Qt::red), QImage(16, 16, QImage::Format_RGB32),
                               straight, QImage(16, 16, QImage::Format_Grayscale8)}) {
    const Result r = run(f, source);
    QVERIFY(r.valid);
    QVERIFY(r.image.size() == QSize(16, 16) && isPremultiplied(r.image));
  }
  // A non-finite or absurd region attribute is an invalid filter, not a crash.
  for (double bad : {kNaN, kInf, -kInf, 1e300, -1e300}) {
    Fixture g;
    exactRegion(g, 16, 16);
    detail::blur(g, 1, 1);
    g.def.region.x = px(bad);
    QVERIFY(!run(g, solid(16, 16, Qt::red)).valid);
    g.def.region.x = px(0);
    g.def.region.height = px(bad);
    QVERIFY(!run(g, solid(16, 16, Qt::red)).valid);
  }
  // A viewport of zero or NaN only matters for percentages.
  Fixture h;
  h.def.region.filterUserSpace = true;
  h.def.region.x = h.def.region.y = px(0);
  h.def.region.width = pct(50);
  h.def.region.height = pct(50);
  h.ctx.viewport = QSizeF(0, 0);
  QVERIFY(!run(h, QImage()).valid);
  h.ctx.viewport = QSizeF(kNaN, 10);
  QVERIFY(!run(h, QImage()).valid);
  // The largest allowed region works and the engine frees what it no longer needs.
  Fixture big;
  big.def.region.filterUserSpace = true;
  big.def.region.x = big.def.region.y = px(0);
  big.def.region.width = big.def.region.height = px(2896);
  flood(big, "#ff0000", 1.0, "a");
  detail::offset(big, 3, 3, "a", "b");
  detail::merge(big, {QStringLiteral("a"), QStringLiteral("b")});
  QElapsedTimer timer;
  timer.start();
  const Result r = run(big, QImage());
  QVERIFY(r.valid);
  QCOMPARE(r.image.size(), QSize(2896, 2896));
  VERIFY_RAW(r, 2895, 2895, 255, 0, 0, 255, 0);
  QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));
}

inline void blurCostIsIndependentOfRadius() {
  const QImage src = withRect(256, 256, QRect(64, 64, 128, 128), Qt::white);
  auto best = [&](double sigma) {
    qint64 least = std::numeric_limits<qint64>::max();
    for (int i = 0; i < 5; ++i) {
      Fixture f;
      exactRegion(f, 256, 256);
      detail::blur(f, sigma, sigma);
      f.store.back().colorSpace = ColorSpace::SRGB;
      QElapsedTimer t;
      t.start();
      const Result r = run(f, src);
      least = r.valid ? std::min(least, t.nsecsElapsed()) : std::numeric_limits<qint64>::max();
    }
    return static_cast<double>(least) / 1.0e6;
  };
  const double small = best(2.5), large = best(60), huge = best(5000);
  qInfo().noquote() << QStringLiteral("BLUR 256x256: sigma 2.5 %1 ms, sigma 60 %2 ms, sigma 5000 %3 ms")
                           .arg(small, 0, 'f', 2).arg(large, 0, 'f', 2).arg(huge, 0, 'f', 2);
  // The running-sum box blur does the same work for any radius. The margin is
  // generous because sanitizer builds and loaded CI machines are noisy.
  QVERIFY2(large < small * 4 + 10, qPrintable(QStringLiteral("%1 vs %2 ms").arg(large).arg(small)));
  QVERIFY2(huge < small * 4 + 10, qPrintable(QStringLiteral("%1 vs %2 ms").arg(huge).arg(small)));
}

inline void applyNeverModifiesItsInput() {
  QStringList failures;
  for (const GoldenCase& g : goldenCases()) {
    Fixture f(g.source);
    g.build(f);
    const QRect region = regionInDevice(f.def, f.ctx);
    QImage src = renderSource(f, region);
    const QImage before = src.copy();
    const qint64 key = src.cacheKey();
    Result r = apply(f.def, f.ctx, src);
    if (!sameBytes(src, before) || src.cacheKey() != key) failures << g.name + QStringLiteral(": apply changed the source");
    // Writing to the result must not reach the source either, even when a
    // disabled primitive returned the input image itself.
    if (r.valid && !r.image.isNull()) {
      r.image.fill(Qt::magenta);
      if (!sameBytes(src, before)) failures << g.name + QStringLiteral(": the result aliases the source");
    }
  }
  QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
}

}  // namespace filtertest
