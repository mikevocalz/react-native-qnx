#pragma once

// Helpers shared by the scene-level tests: building retained element trees
// directly (what the Fabric bridge produces) and probing painted pixels.

#include "RnsvgItems.h"
#include "RnsvgPath.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTest>

using namespace rnqnx::views;

namespace rnqnx::views::rnsvg::testkit {

Length px(double v) { return Length{v, Length::Unit::Px, true}; }
Length pct(double v) { return Length{v, Length::Unit::Percent, true}; }

Brush color(const QColor& c) { return Brush{Brush::Type::Color, c, {}}; }
Brush ref(const QString& id) { return Brush{Brush::Type::Ref, {}, id}; }

ElementProps make(Kind kind) {
  ElementProps p;
  p.kind = kind;
  p.fill = color(Qt::black);
  return p;
}

// A shape that sets its own fill, the way extractFill reports it.
ElementProps filled(Kind kind, const QColor& c) {
  ElementProps p = make(kind);
  p.fill = color(c);
  p.propList = {QStringLiteral("fill")};
  return p;
}

RnsvgElementItem* add(QQuickItem* parent, ElementProps p) {
  const Kind k = p.kind;
  auto* item = new RnsvgElementItem(k, parent);
  item->setProps(std::move(p));
  return item;
}

ElementProps rect(double x, double y, double w, double h, const QColor& c) {
  ElementProps p = filled(Kind::Rect, c);
  p.x = px(x); p.y = px(y); p.width = px(w); p.height = px(h);
  return p;
}

ElementProps circle(double cx, double cy, double r, const QColor& c) {
  ElementProps p = filled(Kind::Circle, c);
  p.cx = px(cx); p.cy = px(cy); p.r = px(r);
  return p;
}

QImage render(RnsvgRootItem& root) {
  const qreal dpr = 1.0;
  QImage image(QSize(qCeil(root.width() * dpr), qCeil(root.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  root.paint(&painter);
  painter.end();
  return image;
}

bool near(const QColor& a, const QColor& b, int tol = 4) {
  return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol &&
         std::abs(a.blue() - b.blue()) <= tol && std::abs(a.alpha() - b.alpha()) <= tol;
}

#define VERIFY_PX(img, x, y, expected)                                                      \
  do {                                                                                      \
    const QColor got = (img).pixelColor(x, y);                                              \
    if (!near(got, expected)) {                                                             \
      QFAIL(qPrintable(QStringLiteral("pixel (%1,%2) is %3, expected %4")                   \
                           .arg(x).arg(y).arg(got.name(QColor::HexArgb), QColor(expected).name(QColor::HexArgb)))); \
    }                                                                                       \
  } while (0)

#define VERIFY_TRANSPARENT(img, x, y) \
  QVERIFY2((img).pixelColor(x, y).alpha() <= 4, qPrintable(QStringLiteral("(%1,%2) should be empty").arg(x).arg(y)))

struct Scene {
  RnsvgRootItem root;
  explicit Scene(double w = 100, double h = 100) {
    root.setSize(QSizeF(w, h));
    RootProps rp;
    root.setRootProps(rp);
  }
};

}  // namespace rnqnx::views::rnsvg::testkit
