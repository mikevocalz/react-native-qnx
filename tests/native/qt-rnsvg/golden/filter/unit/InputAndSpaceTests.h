#pragma once

// Primitive inputs and references, color spaces, scaling and rotation of the transform.
// Each function is one test; RnsvgFilterTest.cpp runs it as a slot. The QtTest
// macros work in any void function, so these are plain functions.

#include "../UnitSupport.h"

#include <QDir>
#include <limits>

namespace filtertest {

inline void standardInputs() {
  Fixture f;
  exactRegion(f, 20, 20);
  const QImage src = solid(20, 20, QColor(255, 0, 0, 128));
  passThrough(f, "SourceGraphic");
  VERIFY_RAW(run(f, src), 5, 5, 128, 0, 0, 128, 1);
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "SourceAlpha");
  VERIFY_RAW(run(f, src), 5, 5, 0, 0, 0, 128, 0);
  for (const char* bg : {"BackgroundImage", "BackgroundAlpha"}) {
    f.store.clear();
    f.def.primitives.clear();
    passThrough(f, bg);
    const Result r = run(f, src);
    QVERIFY2(r.valid && totalAlpha(r.image) == 0.0, bg);
  }
  f.ctx.fillPaint = QColor(0x33, 0x66, 0x99);
  f.ctx.strokePaint = QColor(0x99, 0x66, 0x33, 128);
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "FillPaint");
  VERIFY_RAW(run(f, src), 19, 19, 0x33, 0x66, 0x99, 255, 1);
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "StrokePaint");
  VERIFY_RAW(run(f, src), 0, 0, 0x99 * 128 / 255, 0x66 * 128 / 255, 0x33 * 128 / 255, 128, 1);
  f.ctx.fillPaint = QColor();  // none
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "FillPaint");
  QCOMPARE(totalAlpha(run(f, src).image), 0.0);
}

inline void namedAndImplicitInputs() {
  Fixture f;
  exactRegion(f, 10, 10);
  flood(f, "#ff0000", 1, "a");
  flood(f, "#0000ff", 1, "b");
  passThrough(f, "a");
  VERIFY_RAW(run(f, QImage()), 5, 5, 255, 0, 0, 255, 0);  // a named result, not the previous one
  f.store.clear();
  f.def.primitives.clear();
  flood(f, "#ff0000");
  passThrough(f, "");  // the implicit input is the previous result
  VERIFY_RAW(run(f, QImage()), 5, 5, 255, 0, 0, 255, 0);
  f.store.clear();
  f.def.primitives.clear();
  flood(f, "#ff0000", 1, "r");
  flood(f, "#0000ff", 1, "r");
  passThrough(f, "r");  // a repeated name means the nearest earlier primitive
  VERIFY_RAW(run(f, QImage()), 5, 5, 0, 0, 255, 255, 0);
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "");  // the first primitive reads SourceGraphic
  VERIFY_RAW(run(f, solid(10, 10, Qt::green)), 5, 5, 0, 255, 0, 255, 0);
}

inline void badReferencesFallBack() {
  // An unknown name, a forward reference and a self reference read the previous
  // result, or SourceGraphic for the first primitive (resvg and Filter Effects).
  Fixture f;
  exactRegion(f, 10, 10);
  passThrough(f, "later");  // forward reference to the next primitive's result
  flood(f, "#ff0000", 1, "later");
  QVERIFY(run(f, solid(10, 10, Qt::green)).valid);
  f.def.primitives = {f.def.primitives[1], f.def.primitives[0]};  // flood first, then the reader
  VERIFY_RAW(run(f, solid(10, 10, Qt::green)), 5, 5, 255, 0, 0, 255, 0);  // "later" is now defined earlier
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "later");
  flood(f, "#ff0000", 1, "later");
  VERIFY_RAW(run(f, solid(10, 10, Qt::green)), 5, 5, 255, 0, 0, 255, 0);  // the flood is the last result
  f.store.clear();
  f.def.primitives.clear();
  flood(f, "#0000ff");
  Primitive& self = passThrough(f, "x");
  self.result = QStringLiteral("x");
  VERIFY_RAW(run(f, QImage()), 5, 5, 0, 0, 255, 255, 0);  // "x" is not defined before this primitive
  f.store.clear();
  f.def.primitives.clear();
  passThrough(f, "missing");
  VERIFY_RAW(run(f, solid(10, 10, Qt::green)), 5, 5, 0, 255, 0, 255, 0);
}

inline void emptyAndNullPrimitiveLists() {
  Fixture f;
  exactRegion(f, 10, 10);
  Result r = run(f, solid(10, 10, Qt::red));
  QVERIFY(r.valid);  // an empty filter draws nothing
  QCOMPARE(r.image.size(), QSize(10, 10));
  QCOMPARE(totalAlpha(r.image), 0.0);
  flood(f, "#ff0000");
  f.def.primitives.insert(f.def.primitives.begin(), nullptr);
  f.def.primitives.push_back(nullptr);
  r = run(f, QImage());
  VERIFY_RAW(r, 3, 3, 255, 0, 0, 255, 0);
  f.def.primitives.assign(3, nullptr);
  r = run(f, QImage());
  QVERIFY(r.valid && totalAlpha(r.image) == 0.0);
}

inline void colorInterpolationFilters() {
  // feComponentTransfer halves each channel of a mid gray. In linearRGB the
  // halving happens on light, so the result is brighter than half the sRGB value.
  for (ColorSpace cs : {ColorSpace::LinearRGB, ColorSpace::SRGB}) {
    Fixture f;
    exactRegion(f, 8, 8);
    detail::componentTransfer(f, detail::fn("linear", {}, 0.5), detail::fn("linear", {}, 0.5), detail::fn("linear", {}, 0.5),
                              detail::fn("identity"));
    f.store.back().colorSpace = cs;
    const Result r = run(f, solid(8, 8, QColor(128, 128, 128)));
    if (cs == ColorSpace::SRGB) VERIFY_RAW(r, 4, 4, 64, 64, 64, 255, 1);
    else VERIFY_RAW(r, 4, 4, 93, 93, 93, 255, 3);
  }
  // A flood color is an sRGB value whatever the space the filter works in.
  Fixture f;
  exactRegion(f, 8, 8);
  flood(f, "#808080");
  VERIFY_RAW(run(f, QImage()), 4, 4, 128, 128, 128, 255, 1);
}

inline void offsetKeepsColorsExact() {
  Fixture f;  // linearRGB by default; a pure move needs no conversion
  exactRegion(f, 8, 8);
  detail::offset(f, 1, 0);
  const QImage src = solid(8, 8, QColor(10, 20, 30));
  VERIFY_RAW(run(f, src), 4, 4, 10, 20, 30, 255, 0);
}

inline void lightingColorIsConvertedToWorkingSpace() {
  // Browsers convert lighting-color to the working space; resvg 0.50 does not
  // (it would give 188 here). A light facing the flat surface head on returns
  // the lighting color itself.
  for (ColorSpace cs : {ColorSpace::LinearRGB, ColorSpace::SRGB}) {
    Fixture f;
    exactRegion(f, 8, 8);
    DiffuseLighting d;
    d.lightingColor = detail::c("#808080");
    d.light = detail::distant();
    d.light.elevation = 90;
    Primitive& p = f.add(d);
    p.colorSpace = cs;
    VERIFY_RAW(run(f, solid(8, 8, Qt::black)), 4, 4, 128, 128, 128, 255, 3);
  }
}

inline void blurScalesWithTransform() {
  const QImage block = withRect(100, 100, QRect(20, 20, 60, 60), Qt::white);
  {  // user deviation 3 at scale 2 is the same as device deviation 6
    Fixture scaled, plain;
    exactRegion(scaled, 50, 50);
    scaled.ctx.userToDevice = QTransform::fromScale(2, 2);
    detail::blur(scaled, 3, 3);
    exactRegion(plain, 100, 100);
    detail::blur(plain, 6, 6);
    const Result a = run(scaled, block), b = run(plain, block);
    QCOMPARE(a.rect, QRect(0, 0, 100, 100));
    QVERIFY2(sameBytes(a.image, b.image), "scaled blur differs from the equivalent device blur");
  }
  {  // non-uniform scale: (2, 2) user becomes (4, 2) device
    Fixture scaled, plain;
    exactRegion(scaled, 50, 100);
    scaled.ctx.userToDevice = QTransform::fromScale(2, 1);
    detail::blur(scaled, 2, 2);
    exactRegion(plain, 100, 100);
    detail::blur(plain, 4, 2);
    QVERIFY2(sameBytes(run(scaled, block).image, run(plain, block).image), "non-uniform scale");
  }
  {  // objectBoundingBox primitive units: the deviation is a fraction of the box
    Fixture units, plain;
    exactRegion(units, 100, 100);
    units.def.region.primitiveUserSpace = false;
    detail::blur(units, 0.05, 0.1);  // box is 100x100
    exactRegion(plain, 100, 100);
    detail::blur(plain, 5, 10);
    QVERIFY2(sameBytes(run(units, block).image, run(plain, block).image), "bounding box units");
  }
}

inline void offsetFollowsTransform() {
  Fixture f;
  exactRegion(f, 20, 30);
  f.ctx.userToDevice = QTransform::fromScale(2, 3);  // region is 40x90 on the device
  detail::offset(f, 5, 2);
  const Result r = run(f, withRect(40, 90, QRect(0, 0, 4, 4), Qt::white));
  VERIFY_RAW(r, 10, 6, 255, 255, 255, 255, 0);  // moved by (10, 6)
  VERIFY_RAW(r, 13, 9, 255, 255, 255, 255, 0);
  VERIFY_RAW(r, 14, 9, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 9, 6, 0, 0, 0, 0, 0);
  // Fractions round to the nearest device pixel.
  f.store.clear();
  f.def.primitives.clear();
  detail::offset(f, 2.4, 0);  // 4.8 device pixels
  const Result r2 = run(f, withRect(40, 90, QRect(0, 0, 4, 4), Qt::white));
  VERIFY_RAW(r2, 5, 0, 255, 255, 255, 255, 0);
  VERIFY_RAW(r2, 4, 0, 0, 0, 0, 0, 0);
}

inline void rotatedAndFlippedTransforms() {
  {  // rotate(90): user (dx, 0) goes down the device y axis
    Fixture f;
    f.def.region.filterUserSpace = true;
    f.def.region.x = f.def.region.y = px(0);
    f.def.region.width = px(50);
    f.def.region.height = px(100);
    f.ctx.userToDevice = QTransform().rotate(90);
    f.ctx.objectBoundingBox = QRectF(0, 0, 50, 100);
    QCOMPARE(regionInDevice(f.def, f.ctx), QRect(-100, 0, 100, 50));
    detail::offset(f, 10, 0);
    const QImage src = withRect(100, 50, QRect(10, 10, 10, 10), Qt::white);
    const Result r = run(f, src);
    VERIFY_RAW(r, 15, 25, 255, 255, 255, 255, 0);
    VERIFY_RAW(r, 15, 15, 0, 0, 0, 0, 0);
    VERIFY_RAW(r, 25, 15, 0, 0, 0, 0, 0);
    // A user-space x blur becomes a device y blur.
    f.store.clear();
    f.def.primitives.clear();
    detail::blur(f, 6, 0);
    Fixture g;
    exactRegion(g, 100, 50);
    detail::blur(g, 0, 6);
    QVERIFY(sameBytes(run(f, src).image, run(g, src).image));
  }
  {  // a mirror moves user +x to device -x
    Fixture f;
    f.def.region.filterUserSpace = true;
    f.def.region.x = f.def.region.y = px(0);
    f.def.region.width = px(100);
    f.def.region.height = px(50);
    f.ctx.userToDevice = QTransform::fromScale(-1, 1);
    f.ctx.objectBoundingBox = QRectF(0, 0, 100, 50);
    QCOMPARE(regionInDevice(f.def, f.ctx), QRect(-100, 0, 100, 50));
    detail::offset(f, 10, 0);
    const Result r = run(f, withRect(100, 50, QRect(50, 10, 10, 10), Qt::white));
    VERIFY_RAW(r, 45, 15, 255, 255, 255, 255, 0);
    VERIFY_RAW(r, 55, 15, 0, 0, 0, 0, 0);
  }
  {  // 30 degrees: the result covers the bounding box of the turned region
    Fixture f;
    f.def.region.filterUserSpace = true;
    f.def.region.x = f.def.region.y = px(0);
    f.def.region.width = px(60);
    f.def.region.height = px(40);
    f.ctx.userToDevice = QTransform().rotate(30);
    flood(f, "#ff0000");
    const QRect region = regionInDevice(f.def, f.ctx);
    const Result r = run(f, QImage());
    QVERIFY(r.valid);
    QCOMPARE(r.rect, region);
    QCOMPARE(r.image.size(), region.size());
    QVERIFY(region.width() > 60 && region.height() > 40);
  }
}

}  // namespace filtertest
