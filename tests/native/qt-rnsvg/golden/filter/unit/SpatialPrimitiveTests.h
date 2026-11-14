#pragma once

// Primitives that read or place pixels by position: tile, image, convolve matrix, displacement map, lighting, turbulence.
// Each function is one test; RnsvgFilterTest.cpp runs it as a slot. The QtTest
// macros work in any void function, so these are plain functions.

#include "../UnitSupport.h"

#include <QDir>
#include <limits>

namespace filtertest {

inline void tileRepeatsTheInputSubregion() {
  Fixture f;
  exactRegion(f, 20, 20);
  detail::offset(f, 0, 0, "SourceGraphic", "t");
  Primitive& tile = f.store.back();
  tile.subregion.x = tile.subregion.y = px(2);
  tile.subregion.width = tile.subregion.height = px(4);
  f.add(Tile{}).in1 = QStringLiteral("t");
  QImage src(20, 20, QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < 20; ++y) {
    for (int x = 0; x < 20; ++x) reinterpret_cast<QRgb*>(src.scanLine(y))[x] = qRgba(x * 10, y * 10, 0, 255);
  }
  const Result r = run(f, src);
  auto expectFrom = [&](int x, int y, int sx, int sy) { VERIFY_RAW(r, x, y, sx * 10, sy * 10, 0, 255, 0); };
  expectFrom(2, 2, 2, 2);
  expectFrom(5, 5, 5, 5);
  expectFrom(6, 2, 2, 2);    // next tile to the right
  expectFrom(9, 9, 5, 5);
  expectFrom(0, 0, 4, 4);    // tiles also extend up and left: 0 is two before 2, phase 2 of 4
  expectFrom(1, 3, 5, 3);
  expectFrom(19, 19, 3, 3);
}

inline void imagePlacement() {
  // A 4x2 picture: columns red, green, blue, white in the top row; the bottom row black.
  QImage pic(4, 2, QImage::Format_ARGB32);
  const QColor cols[4] = {Qt::red, Qt::green, Qt::blue, Qt::white};
  for (int x = 0; x < 4; ++x) {
    pic.setPixelColor(x, 0, cols[x]);
    pic.setPixelColor(x, 1, Qt::black);
  }
  auto place = [&](const char* align, int meetOrSlice) {
    auto f = std::make_unique<Fixture>();
    exactRegion(*f, 60, 60);
    Image im;
    im.href = QStringLiteral("x.png");
    im.align = QLatin1String(align);
    im.meetOrSlice = meetOrSlice;
    Primitive& p = f->add(im);
    p.subregion.x = p.subregion.y = px(10);
    p.subregion.width = p.subregion.height = px(40);
    f->ctx.loadImage = [pic](const QString&) { return pic; };
    return std::make_pair(std::move(f), 0);
  };
  auto render = [&](const char* align, int meetOrSlice) {
    auto made = place(align, meetOrSlice);
    return run(*made.first, QImage());
  };
  // meet, centered: 4x2 scales by 10 to 40x20 and sits at y = 20..40.
  Result r = render("xMidYMid", 0);
  VERIFY_RAW(r, 15, 25, 255, 0, 0, 255, 30);  // bilinear sampling mixes in the neighbours near cell centres
  VERIFY_RAW(r, 25, 25, 0, 255, 0, 255, 30);
  VERIFY_RAW(r, 35, 25, 0, 0, 255, 255, 30);
  VERIFY_RAW(r, 45, 25, 255, 255, 255, 255, 30);
  VERIFY_RAW(r, 15, 35, 0, 0, 0, 255, 30);
  VERIFY_RAW(r, 15, 15, 0, 0, 0, 0, 0);  // above the picture, inside the subregion
  VERIFY_RAW(r, 5, 25, 0, 0, 0, 0, 0);   // outside the subregion
  r = render("xMinYMin", 0);             // top-aligned: y = 10..30
  VERIFY_RAW(r, 15, 15, 255, 0, 0, 255, 30);
  VERIFY_RAW(r, 15, 35, 0, 0, 0, 0, 0);
  r = render("xMaxYMax", 0);             // bottom-aligned: y = 30..50
  VERIFY_RAW(r, 15, 45, 0, 0, 0, 255, 30);
  VERIFY_RAW(r, 15, 25, 0, 0, 0, 0, 0);
  r = render("xMidYMid", 1);             // slice: scale 20 -> 80x40, columns 20 wide, clipped to the subregion
  VERIFY_RAW(r, 20, 20, 0, 255, 0, 255, 30);   // x 10..50 shows scaled columns 1 and 2
  VERIFY_RAW(r, 40, 20, 0, 0, 255, 255, 30);
  VERIFY_RAW(r, 5, 15, 0, 0, 0, 0, 0);
  r = render("none", 2);                 // stretched: 10 wide columns, 20 tall rows
  VERIFY_RAW(r, 15, 20, 255, 0, 0, 255, 30);
  VERIFY_RAW(r, 15, 40, 0, 0, 0, 255, 30);
  // No loader, an unknown element or an undecodable image leave the result transparent.
  Fixture f;
  exactRegion(f, 20, 20);
  Image im;
  im.href = QStringLiteral("x.png");
  f.add(im);
  QCOMPARE(totalAlpha(run(f, QImage()).image), 0.0);
  f.ctx.loadImage = [](const QString&) { return QImage(); };
  QCOMPARE(totalAlpha(run(f, QImage()).image), 0.0);
  f.store.back().params = Image{QStringLiteral("#nothing"), QStringLiteral("xMidYMid"), 0};
  f.ctx.renderElement = [](const QString&, const QTransform&, const QRect&) { return QImage(); };
  QCOMPARE(totalAlpha(run(f, QImage()).image), 0.0);
  // The picture follows the transform.
  Fixture g;
  g.def.region.filterUserSpace = true;
  g.def.region.x = g.def.region.y = px(0);
  g.def.region.width = g.def.region.height = px(10);
  g.ctx.userToDevice = QTransform::fromScale(4, 4);
  g.ctx.loadImage = [](const QString&) {
    QImage two(2, 2, QImage::Format_ARGB32);
    two.fill(Qt::red);
    two.setPixelColor(1, 1, Qt::blue);
    return two;
  };
  Image big;
  big.href = QStringLiteral("x.png");
  g.add(big);
  const Result rg = run(g, QImage());
  QCOMPARE(rg.image.size(), QSize(40, 40));
  VERIFY_RAW(rg, 5, 5, 255, 0, 0, 255, 30);
  VERIFY_RAW(rg, 35, 35, 0, 0, 255, 255, 30);
}

inline void convolveMatrixSemantics() {
  QImage src(8, 1, QImage::Format_ARGB32_Premultiplied);
  src.fill(Qt::transparent);
  reinterpret_cast<QRgb*>(src.scanLine(0))[5] = qRgba(255, 255, 255, 255);
  reinterpret_cast<QRgb*>(src.scanLine(0))[7] = qRgba(255, 255, 255, 255);
  // Kernel {1,0,0} in a 3x1 matrix: kernelMatrix runs backwards against the
  // source, so the result at x is the source at x + 1.
  auto convolve = [&](EdgeMode edge, std::vector<double> k = {1, 0, 0}, double divisor = 1, bool preserve = false) {
    Fixture f;
    exactRegion(f, 8, 1);
    const int orderX = static_cast<int>(k.size());
    detail::convolve(f, orderX, 1, std::move(k), divisor, edge, preserve);
    return run(f, src);
  };
  Result r = convolve(EdgeMode::None);
  VERIFY_RAW(r, 4, 0, 255, 255, 255, 255, 1);
  VERIFY_RAW(r, 5, 0, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 6, 0, 255, 255, 255, 255, 1);
  VERIFY_RAW(r, 7, 0, 0, 0, 0, 0, 0);                       // reads past the edge: transparent
  VERIFY_RAW(convolve(EdgeMode::Duplicate), 7, 0, 255, 255, 255, 255, 1);  // repeats the last pixel
  src.fill(Qt::transparent);
  reinterpret_cast<QRgb*>(src.scanLine(0))[0] = qRgba(255, 255, 255, 255);
  VERIFY_RAW(convolve(EdgeMode::Wrap), 7, 0, 255, 255, 255, 255, 1);   // wraps to pixel 0
  VERIFY_RAW(convolve(EdgeMode::None), 7, 0, 0, 0, 0, 0, 0);
  // targetX moves the origin: target 0 reads the source at x + 2.
  Fixture f;
  exactRegion(f, 8, 1);
  detail::convolve(f, 3, 1, {1, 0, 0}, 1, EdgeMode::None, false, 0, 0);
  reinterpret_cast<QRgb*>(src.scanLine(0))[0] = qRgba(0, 0, 0, 0);
  reinterpret_cast<QRgb*>(src.scanLine(0))[5] = qRgba(255, 255, 255, 255);
  VERIFY_RAW(run(f, src), 3, 0, 255, 255, 255, 255, 1);
  // divisor 0 means the kernel sum (or 1 when that is 0): a 1 1 1 average.
  QImage flat = solid(8, 1, QColor(90, 90, 90));
  Fixture g;
  exactRegion(g, 8, 1);
  detail::convolve(g, 3, 1, {1, 1, 1}, 0, EdgeMode::Duplicate);
  VERIFY_RAW(run(g, flat), 3, 0, 90, 90, 90, 255, 1);
  // preserveAlpha convolves demultiplied color and keeps the source alpha.
  QImage half = solid(8, 1, QColor(200, 100, 50, 128));
  Fixture h;
  exactRegion(h, 8, 1);
  detail::convolve(h, 3, 1, {0, 1, 0}, 1, EdgeMode::Duplicate, true);
  h.store.back().colorSpace = ColorSpace::SRGB;
  const QRgb px4 = raw(run(h, half).image, 4, 0);
  QVERIFY2(close(px4, 100, 50, 25, 128, 2), qPrintable(describe(px4)));
}

inline void displacementMapChannels() {
  // A flood map with R=255, G=0, B=255, A=255 displaces by +scale/2 in x for
  // R or B and by -scale/2 in y for G; out(x,y) = in(x + dx, y + dy).
  auto displaced = [&](const char* xc, const char* yc) {
    Fixture f;
    exactRegion(f, 30, 30);
    flood(f, "#ff00ff", 1.0, "t");
    detail::displacement(f, 10, xc, yc);
    return run(f, withRect(30, 30, QRect(10, 10, 1, 1), Qt::white));
  };
  Result r = displaced("R", "G");  // dx +5, dy -5: the pixel at (10,10) lands at (5,15)
  VERIFY_RAW(r, 5, 15, 255, 255, 255, 255, 0);
  QCOMPARE(totalAlpha(r.image), 255.0);
  r = displaced("G", "B");         // dx -5, dy +5: lands at (15,5)
  VERIFY_RAW(r, 15, 5, 255, 255, 255, 255, 0);
  r = displaced("A", "A");         // A = 1: dx = dy = +5: lands at (5,5)
  VERIFY_RAW(r, 5, 5, 255, 255, 255, 255, 0);
  // Samples that fall outside the region are transparent.
  Fixture f;
  exactRegion(f, 30, 30);
  flood(f, "#ff00ff", 1.0, "t");
  detail::displacement(f, 100, "R", "R");
  QCOMPARE(totalAlpha(run(f, solid(30, 30, Qt::white)).image), 0.0);
}

inline void lightingOnFlatSurface() {
  auto diffuse = [&](double elevation, LightSource light, const char* hex = "#ffffff", int size = 100) {
    Fixture f;
    exactRegion(f, size, size);
    DiffuseLighting d;
    d.lightingColor = detail::c(hex);
    light.elevation = elevation;
    d.light = light;
    f.add(d).colorSpace = ColorSpace::SRGB;
    return run(f, solid(size, size, Qt::white));
  };
  LightSource dist = detail::distant();
  dist.azimuth = 0;
  Result r = diffuse(90, dist);
  VERIFY_RAW(r, 50, 50, 255, 255, 255, 255, 1);
  r = diffuse(45, dist);  // N.L = sin 45
  VERIFY_RAW(r, 50, 50, 180, 180, 180, 255, 2);
  r = diffuse(0, dist);
  VERIFY_RAW(r, 50, 50, 0, 0, 0, 255, 0);
  r = diffuse(90, dist, "#ff8000");
  VERIFY_RAW(r, 50, 50, 255, 128, 0, 255, 1);
  // A point light at (50,50,50) above the middle: head on there, 45 degrees at (0,50).
  LightSource point = detail::point();
  point.x = point.y = point.z = 50;
  r = diffuse(0, point);
  VERIFY_RAW(r, 50, 50, 255, 255, 255, 255, 1);
  VERIFY_RAW(r, 0, 50, 180, 180, 180, 255, 3);
  // A spot light pointing straight down with a 20 degree cone: black outside it.
  LightSource spot = detail::spot(1, 20);
  spot.x = spot.y = spot.z = 50;
  spot.pointsAtX = spot.pointsAtY = 50;
  spot.pointsAtZ = 0;
  r = diffuse(0, spot);
  VERIFY_RAW(r, 50, 50, 255, 255, 255, 255, 1);
  VERIFY_RAW(r, 90, 50, 0, 0, 0, 255, 0);
  const int inside = qRed(raw(r.image, 60, 50));
  QVERIFY2(inside > 230 && inside < 255, qPrintable(QString::number(inside)));
  // Specular: alpha is the strongest channel, colors are stored premultiplied.
  auto specular = [&](double elevation, double exponent, const char* hex) {
    Fixture f;
    exactRegion(f, 20, 20);
    SpecularLighting s;
    s.specularExponent = exponent;
    s.lightingColor = detail::c(hex);
    s.light = dist;
    s.light.elevation = elevation;
    f.add(s).colorSpace = ColorSpace::SRGB;
    return run(f, solid(20, 20, Qt::white));
  };
  r = specular(90, 1, "#ff8000");
  VERIFY_RAW(r, 10, 10, 255, 128, 0, 255, 1);
  r = specular(45, 2, "#ffffff");  // H is 22.5 degrees off the normal: cos^2 = 0.854
  VERIFY_RAW(r, 10, 10, 218, 218, 218, 218, 2);
  QVERIFY(isPremultiplied(r.image));
}

inline void turbulenceProperties() {
  auto noise = [&](double seed, int octaves, const char* type = "turbulence", double fx = 0.08, bool stitch = false) {
    Fixture f;
    exactRegion(f, 40, 40);
    detail::turbulence(f, type, fx, fx, octaves, seed, stitch);
    return run(f, QImage()).image;
  };
  QVERIFY(sameBytes(noise(3, 2), noise(3, 2)));
  QVERIFY(!sameBytes(noise(3, 2), noise(4, 2)));
  QVERIFY(!sameBytes(noise(3, 2), noise(3, 3)));
  QVERIFY2(sameBytes(noise(2.9, 2), noise(2, 2)), "the seed truncates toward zero");
  QVERIFY(sameBytes(noise(-2.9, 2), noise(-2, 2)));
  QVERIFY(sameBytes(noise(0, 2), noise(0.9, 2)));
  QVERIFY2(sameBytes(noise(1, 24), noise(1, 100000)), "octaves are capped");
  QVERIFY(sameBytes(noise(1, 0), noise(1, -7)));
  QCOMPARE(totalAlpha(noise(1, 0)), 0.0);  // no octaves: turbulence is 0 everywhere
  QCOMPARE(totalAlpha(noise(1, 2, "turbulence", 0.0)), 0.0);
  const QImage fractal = noise(5, 3, "fractalNoise");
  QVERIFY(isPremultiplied(fractal));
  QVERIFY(totalAlpha(fractal) > 0);
  // fractalNoise is centered on 0.5, so the mean alpha sits near half.
  const double meanAlpha = totalAlpha(fractal) / (40.0 * 40.0);
  QVERIFY2(meanAlpha > 90 && meanAlpha < 165, qPrintable(QString::number(meanAlpha)));
  // Noise with a negative frequency is an error: transparent black.
  QCOMPARE(totalAlpha(noise(1, 2, "turbulence", -0.1)), 0.0);
  // stitchTiles makes the tile edge continuous when the tile repeats. Compare the
  // step across the seam of a feTile repetition with the step inside a tile.
  auto seamRatio = [&](bool stitch) {
    Fixture f;
    exactRegion(f, 200, 20);
    detail::turbulence(f, "fractalNoise", 0.031, 0.031, 1, 11, stitch);
    Primitive& t = f.store.back();
    t.result = QStringLiteral("t");
    t.subregion.x = t.subregion.y = px(0);
    t.subregion.width = px(100);
    t.subregion.height = px(20);
    f.add(Tile{}).in1 = QStringLiteral("t");
    f.store.back().colorSpace = ColorSpace::SRGB;
    const QImage im = run(f, QImage()).image;
    double seam = 0, inner = 0;
    for (int y = 0; y < 20; ++y) {
      seam += std::abs(qRed(raw(im, 99, y)) - qRed(raw(im, 100, y))) + std::abs(qAlpha(raw(im, 99, y)) - qAlpha(raw(im, 100, y)));
      for (int x = 20; x < 80; ++x) {
        inner += std::abs(qRed(raw(im, x, y)) - qRed(raw(im, x + 1, y))) + std::abs(qAlpha(raw(im, x, y)) - qAlpha(raw(im, x + 1, y)));
      }
    }
    return (seam / 20.0) / (inner / (20.0 * 60.0));
  };
  const double stitched = seamRatio(true), plain = seamRatio(false);
  QVERIFY2(stitched < 3.0 && plain > 3.0 * stitched,
           qPrintable(QStringLiteral("stitched seam ratio %1, plain %2").arg(stitched).arg(plain)));
}

}  // namespace filtertest
