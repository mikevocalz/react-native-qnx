#pragma once

// Filter region and primitive subregion math.
// Each function is one test; RnsvgFilterTest.cpp runs it as a slot. The QtTest
// macros work in any void function, so these are plain functions.

#include "../UnitSupport.h"

#include <QDir>
#include <limits>

namespace filtertest {

inline void regionDefaultsAroundBoundingBox() {
  Fixture f(Source::Red);  // box (40,40,120,120); -10%/120% of it
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(28, 28, 144, 144));
  f.def.region.x = f.def.region.y = pct(0);
  f.def.region.width = f.def.region.height = pct(100);
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(40, 40, 120, 120));
  f.def.region.x = px(-0.5);  // in objectBoundingBox units a bare number is a fraction
  f.def.region.width = px(2);
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(-20, 40, 240, 120));
}

inline void regionInUserSpaceUnits() {
  Fixture f;
  f.def.region.filterUserSpace = true;
  f.def.region.x = px(10);
  f.def.region.y = px(20);
  f.def.region.width = pct(50);   // of the 200x200 viewport
  f.def.region.height = pct(25);
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(10, 20, 100, 50));
  f.def.region.width = Length{2, Length::Unit::Em, true};
  f.ctx.fontSize = 10;
  QCOMPARE(regionInDevice(f.def, f.ctx).width(), 20);
}

inline void regionSnapsOriginDownAndSizeUp() {
  Fixture f;
  f.def.region.filterUserSpace = true;
  f.def.region.x = px(10.7);
  f.def.region.y = px(-0.2);
  f.def.region.width = px(20.2);
  f.def.region.height = px(10);
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(10, -1, 21, 10));
}

inline void regionFollowsScaleAndRotation() {
  Fixture f;
  f.def.region.filterUserSpace = true;
  f.def.region.x = f.def.region.y = px(0);
  f.def.region.width = px(100);
  f.def.region.height = px(50);
  f.ctx.userToDevice = QTransform().translate(5, 7).scale(2, 3);
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(5, 7, 200, 150));
  f.ctx.userToDevice = QTransform().rotate(90);  // (x,y) -> (-y,x)
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(-50, 0, 50, 100));
  f.ctx.userToDevice = QTransform().rotate(45);  // the bounding box of the turned rectangle
  const QRect r = regionInDevice(f.def, f.ctx);
  QVERIFY2(r.width() >= 106 && r.width() <= 108 && r.height() >= 106 && r.height() <= 108, qPrintable(QString::number(r.width())));
}

inline void degenerateRegionsAreInvalid() {
  Fixture f(Source::Red);
  auto empty = [&](const char* what) { QVERIFY2(regionInDevice(f.def, f.ctx).isEmpty(), what); };
  f.ctx.objectBoundingBox = QRectF(10, 10, 0, 50);  // a vertical line has no box for objectBoundingBox units
  empty("zero-width box");
  QVERIFY(!run(f, solid(10, 10, Qt::red)).valid);
  f.ctx.objectBoundingBox = QRectF();
  empty("no box");
  f.def.region.filterUserSpace = true;  // user-space units do not need the box
  f.def.region.x = f.def.region.y = px(0);
  f.def.region.width = f.def.region.height = px(10);
  QVERIFY(!regionInDevice(f.def, f.ctx).isEmpty());
  f.def.region.primitiveUserSpace = false;  // primitiveUnits=objectBoundingBox needs it again
  empty("primitive units need the box");
  f.def.region.primitiveUserSpace = true;
  for (double bad : {0.0, -5.0, kNaN, kInf}) {
    f.def.region.width = px(bad);
    empty("bad width");
  }
  f.def.region.width = px(10);
  f.def.region.x = px(kNaN);
  empty("NaN x");
  f.def.region.x = px(1e300);
  empty("far away x");
  f.def.region.x = px(0);
  f.ctx.userToDevice = QTransform::fromScale(0, 1);
  empty("singular transform");
  f.ctx.userToDevice = QTransform(1, 0, 0, 1, kNaN, 0);
  empty("NaN translation");
  f.ctx.userToDevice = QTransform::fromScale(1e-300, 1e-300).scale(1e300, 1e300);  // finite but absurd
  QVERIFY(regionInDevice(f.def, f.ctx).width() <= 10 + 1);
}

inline void regionCapIsExact() {
  Fixture f;
  f.def.region.filterUserSpace = true;
  f.def.region.x = f.def.region.y = px(0);
  f.def.region.width = f.def.region.height = px(2896);  // 8,386,816 pixels
  QCOMPARE(regionInDevice(f.def, f.ctx), QRect(0, 0, 2896, 2896));
  f.def.region.width = f.def.region.height = px(2897);  // 8,392,609 > kMaxRegionPixels
  QVERIFY(regionInDevice(f.def, f.ctx).isEmpty());
  f.def.region.width = px(100000);
  f.def.region.height = px(80);  // 8,000,000: wide and thin is fine
  QCOMPARE(regionInDevice(f.def, f.ctx).size(), QSize(100000, 80));
  QVERIFY(!run(f, QImage()).valid || run(f, QImage()).image.size() == QSize(100000, 80));
}

inline void subregionDefaultsToFilterRegion() {
  Fixture f;
  exactRegion(f, 100, 80);
  flood(f, "#ff0000");
  const Result r = run(f, solid(100, 80, Qt::transparent));
  QVERIFY(r.valid);
  QCOMPARE(r.rect, QRect(0, 0, 100, 80));
  VERIFY_RAW(r, 0, 0, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 99, 79, 255, 0, 0, 255, 0);
}

inline void subregionExplicitAndPartial() {
  Fixture f;
  exactRegion(f, 100, 80);
  Primitive& p = flood(f, "#ff0000");
  p.subregion.x = px(30);  // width and height default to the region's, so the right part is cut by the region
  Result r = run(f, solid(100, 80, Qt::transparent));
  VERIFY_RAW(r, 29, 40, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 30, 40, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 99, 40, 255, 0, 0, 255, 0);
  p.subregion.x = px(10);
  p.subregion.y = px(20);
  p.subregion.width = px(30);
  p.subregion.height = px(40);
  r = run(f, solid(100, 80, Qt::transparent));
  VERIFY_RAW(r, 9, 30, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 10, 20, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 39, 59, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 40, 59, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 39, 60, 0, 0, 0, 0, 0);
  p.subregion.x = px(-50);  // sticks out of the region on the left
  p.subregion.width = px(80);
  r = run(f, solid(100, 80, Qt::transparent));
  VERIFY_RAW(r, 0, 30, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 29, 30, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 30, 30, 0, 0, 0, 0, 0);
}

inline void subregionPercentAndBoundingBoxUnits() {
  for (int units = 0; units < 2; ++units) {
    Fixture f;
    exactRegion(f, 100, 80);  // viewport and box are both 100x80
    f.def.region.primitiveUserSpace = units == 0;
    Primitive& p = flood(f, "#ff0000");
    p.subregion.x = pct(25);  // 25% of the viewport, or a quarter of the box
    p.subregion.y = pct(50);
    p.subregion.width = pct(50);
    p.subregion.height = pct(25);
    const Result r = run(f, solid(100, 80, Qt::transparent));
    VERIFY_RAW(r, 24, 50, 0, 0, 0, 0, 0);
    VERIFY_RAW(r, 25, 40, 255, 0, 0, 255, 0);
    VERIFY_RAW(r, 74, 59, 255, 0, 0, 255, 0);
    VERIFY_RAW(r, 75, 59, 0, 0, 0, 0, 0);
    VERIFY_RAW(r, 50, 60, 0, 0, 0, 0, 0);
  }
  Fixture f;  // bare numbers in objectBoundingBox units are fractions
  exactRegion(f, 100, 80);
  f.def.region.primitiveUserSpace = false;
  Primitive& p = flood(f, "#ff0000");
  p.subregion.x = px(0.5);
  p.subregion.width = px(0.25);
  const Result r = run(f, solid(100, 80, Qt::transparent));
  VERIFY_RAW(r, 49, 10, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 50, 10, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 74, 10, 255, 0, 0, 255, 0);
  VERIFY_RAW(r, 75, 10, 0, 0, 0, 0, 0);
}

inline void subregionWithNoAreaIsTransparent() {
  for (double size : {0.0, -4.0, kNaN}) {
    Fixture f;
    exactRegion(f, 40, 40);
    Primitive& p = flood(f, "#ff0000");
    p.subregion.width = px(size);
    const Result r = run(f, solid(40, 40, Qt::green));
    QVERIFY(r.valid);
    QCOMPARE(totalAlpha(r.image), 0.0);
  }
}

inline void resultIsClippedToSubregion() {
  Fixture f;
  exactRegion(f, 40, 40);
  Primitive& p = passThrough(f, "SourceGraphic");
  p.subregion.x = p.subregion.y = px(10);
  p.subregion.width = p.subregion.height = px(20);
  const Result r = run(f, solid(40, 40, Qt::blue));
  VERIFY_RAW(r, 9, 15, 0, 0, 0, 0, 0);
  VERIFY_RAW(r, 10, 10, 0, 0, 255, 255, 0);
  VERIFY_RAW(r, 29, 29, 0, 0, 255, 255, 0);
  VERIFY_RAW(r, 30, 29, 0, 0, 0, 0, 0);
  QCOMPARE(totalAlpha(r.image), 20.0 * 20.0 * 255.0);
}

}  // namespace filtertest
