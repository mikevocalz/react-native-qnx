#pragma once

// The golden cases. Each one pairs a <name>.svg in this directory (rendered
// by resvg 0.50.0 into <name>.png by scripts/rnsvg-filter-goldens.sh) with the
// same filter written as engine primitives. The SVG and the builder must be
// kept in step by hand.
//
// Tolerances: a pixel counts as "off" when a channel differs by more than
// `threshold` out of 255. A case passes when the fraction of off pixels is at
// most `maxOver` and the mean absolute channel difference is at most `maxMean`.
// Edge pixels of shapes differ between Qt's and resvg's anti-aliasing, so the
// defaults allow a thin fringe but not a displaced or wrongly colored region.
// Cases that need more say why next to the entry.

#include "FilterTestSupport.h"

#include <QByteArray>
#include <functional>
#include <vector>

namespace filtertest {

struct GoldenCase {
  QString name;
  Source source;
  std::function<void(Fixture&)> build;
  int threshold{12};
  double maxOver{0.0025};  // 0.25% of the pixels
  double maxMean{0.5};
};

// feImage: the 120x120 picture the data URL in image_data.svg holds.
inline QImage testPicture() {
  QImage im(120, 120, QImage::Format_ARGB32);
  for (int y = 0; y < 120; ++y) {
    for (int x = 0; x < 120; ++x) {
      int r = x * 255 / 119, g = y * 255 / 119, b = 128, a = 255;
      if (x < 4 || y < 4 || x >= 116 || y >= 116) r = g = b = 20;
      if (x >= 50 && x < 70 && y >= 50 && y < 70) a = 90;
      im.setPixel(x, y, qRgba(r, g, b, a));
    }
  }
  return im;
}

// feImage with an element reference: the element id="el" of the SVGs.
inline QImage renderEl(const QString& id, const QTransform& toDevice, const QRect& rect) {
  if (id != QLatin1String("el")) return {};
  QImage image(rect.size(), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setTransform(toDevice * QTransform::fromTranslate(-rect.x(), -rect.y()));
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0x20, 0xa0, 0x40));
  p.drawRect(QRectF(60, 70, 50, 40));
  return image;
}

namespace detail {

inline Brush c(const char* hex) { return colorBrush(QColor(QLatin1String(hex))); }

inline void inSrgb(Fixture& f) {
  for (auto& p : f.store) p.colorSpace = ColorSpace::SRGB;
}

inline TransferFunction fn(const char* type, std::vector<double> table = {}, double slope = 1, double intercept = 0,
                           double amplitude = 1, double exponent = 1, double offset = 0) {
  TransferFunction t;
  t.type = QLatin1String(type);
  t.tableValues = std::move(table);
  t.slope = slope;
  t.intercept = intercept;
  t.amplitude = amplitude;
  t.exponent = exponent;
  t.offset = offset;
  return t;
}

inline void blur(Fixture& f, double sx, double sy, const char* in = "", const char* result = "") {
  GaussianBlur b;
  b.stdDeviationX = sx;
  b.stdDeviationY = sy;
  Primitive& p = f.add(b);
  p.in1 = QLatin1String(in);
  p.result = QLatin1String(result);
}

inline void offset(Fixture& f, double dx, double dy, const char* in = "", const char* result = "") {
  Offset o;
  o.dx = px(dx);
  o.dy = px(dy);
  Primitive& p = f.add(o);
  p.in1 = QLatin1String(in);
  p.result = QLatin1String(result);
}

inline void flood(Fixture& f, const char* hex, double opacity, const char* result = "") {
  Flood fl;
  fl.color = c(hex);
  fl.opacity = opacity;
  f.add(fl).result = QLatin1String(result);
}

inline void merge(Fixture& f, std::vector<QString> nodes) {
  Merge m;
  m.nodes = std::move(nodes);
  f.add(m);
}

inline void colorMatrix(Fixture& f, const char* type, std::vector<double> values) {
  ColorMatrix m;
  m.type = QLatin1String(type);
  m.values = std::move(values);
  f.add(m);
}

inline void composite(Fixture& f, const char* op, double k1 = 0, double k2 = 0, double k3 = 0, double k4 = 0) {
  Composite cp;
  cp.in2 = QStringLiteral("o");
  cp.op = QLatin1String(op);
  cp.k1 = k1;
  cp.k2 = k2;
  cp.k3 = k3;
  cp.k4 = k4;
  Primitive& p = f.add(cp);
  p.in1 = QStringLiteral("SourceGraphic");
}

inline void blend(Fixture& f, const char* mode) {
  flood(f, "#80c0a0", 0.8, "f");
  Blend b;
  b.in2 = QStringLiteral("f");
  b.mode = QLatin1String(mode);
  f.add(b).in1 = QStringLiteral("SourceGraphic");
}

inline void morph(Fixture& f, const char* op, double rx, double ry) {
  Morphology m;
  m.op = QLatin1String(op);
  m.radiusX = px(rx);
  m.radiusY = px(ry);
  f.add(m);
}

inline void turbulence(Fixture& f, const char* type, double fx, double fy, int octaves, double seed, bool stitch = false) {
  Turbulence t;
  t.type = QLatin1String(type);
  t.baseFrequencyX = fx;
  t.baseFrequencyY = fy;
  t.numOctaves = octaves;
  t.seed = seed;
  t.stitchTiles = stitch;
  f.add(t).result = QStringLiteral("t");
}

inline void displacement(Fixture& f, double scale, const char* xc, const char* yc) {
  DisplacementMap d;
  d.in2 = QStringLiteral("t");
  d.scale = scale;
  d.xChannelSelector = QLatin1String(xc);
  d.yChannelSelector = QLatin1String(yc);
  f.add(d).in1 = QStringLiteral("SourceGraphic");
}

inline void componentTransfer(Fixture& f, TransferFunction r, TransferFunction g, TransferFunction b, TransferFunction a) {
  ComponentTransfer t;
  t.r = std::move(r);
  t.g = std::move(g);
  t.b = std::move(b);
  t.a = std::move(a);
  f.add(t);
}

inline void bigRegion(Fixture& f) {
  f.def.region.x = pct(-20);
  f.def.region.y = pct(-20);
  f.def.region.width = pct(150);
  f.def.region.height = pct(150);
}

inline void convolve(Fixture& f, int ox, int oy, std::vector<double> k, double divisor, EdgeMode edge, bool preserve = false,
                     int tx = -1, int ty = -1) {
  ConvolveMatrix m;
  m.orderX = ox;
  m.orderY = oy;
  m.kernelMatrix = std::move(k);
  m.divisor = divisor;
  m.edgeMode = edge;
  m.preserveAlpha = preserve;
  m.targetX = tx;
  m.targetY = ty;
  f.add(m);
}

inline LightSource distant() {
  LightSource l;
  l.kind = LightSource::Kind::Distant;
  l.azimuth = 45;
  l.elevation = 50;
  return l;
}
inline LightSource point() {
  LightSource l;
  l.kind = LightSource::Kind::Point;
  l.x = 60;
  l.y = 40;
  l.z = 60;
  return l;
}
inline LightSource spot(double exponent, double cone) {
  LightSource l;
  l.kind = LightSource::Kind::Spot;
  l.x = 40;
  l.y = 30;
  l.z = 80;
  l.pointsAtX = 100;
  l.pointsAtY = 100;
  l.pointsAtZ = 0;
  l.specularExponent = exponent;
  l.limitingConeAngle = cone;
  l.hasLimitingCone = true;
  return l;
}

inline void diffuse(Fixture& f, const char* color, LightSource light) {
  blur(f, 4, 4, "SourceAlpha", "bump");
  DiffuseLighting d;
  d.surfaceScale = 5;
  d.diffuseConstant = 1;
  d.lightingColor = c(color);
  d.light = light;
  f.add(d).in1 = QStringLiteral("bump");
}

inline void specular(Fixture& f, const char* color, double constant, double exponent, LightSource light) {
  blur(f, 4, 4, "SourceAlpha", "bump");
  SpecularLighting s;
  s.surfaceScale = 5;
  s.specularConstant = constant;
  s.specularExponent = exponent;
  s.lightingColor = c(color);
  s.light = light;
  f.add(s).in1 = QStringLiteral("bump");
}

}  // namespace detail

inline std::vector<GoldenCase> goldenCases() {
  using namespace detail;
  using S = Source;
  std::vector<GoldenCase> v;
  auto add = [&](const char* name, S s, std::function<void(Fixture&)> b) {
    v.push_back({QLatin1String(name), s, std::move(b), 12, 0.0025, 0.5});
  };

  // Blur. The box blur (deviation >= 2) uses three boxes where resvg uses
  // five, and the kernel path uses a sampled Gaussian where resvg uses an IIR
  // filter, so the slopes differ by a few levels.
  add("blur_box_5", S::Red, [](Fixture& f) { blur(f, 5, 5); });
  add("blur_kernel_1", S::Red, [](Fixture& f) { blur(f, 1, 1); });
  add("blur_xy", S::Red, [](Fixture& f) { blur(f, 8, 2); });
  add("blur_axis_zero", S::Red, [](Fixture& f) { blur(f, 6, 0); });
  add("blur_srgb", S::TwoShapes, [](Fixture& f) { blur(f, 4, 4); inSrgb(f); });
  add("blur_negative", S::Red, [](Fixture& f) { blur(f, -3, -3); });

  add("offset", S::Red, [](Fixture& f) { offset(f, 20, 10); });
  add("flood_subregion", S::Red, [](Fixture& f) {
    Flood fl;
    fl.color = c("#2080ff");
    fl.opacity = 0.5;
    Primitive& p = f.add(fl);
    p.subregion.x = px(60);
    p.subregion.y = px(50);
    p.subregion.width = px(80);
    p.subregion.height = px(60);
  });
  add("flood_merge", S::TwoShapes, [](Fixture& f) {
    flood(f, "#00c000", 0.5, "f");
    merge(f, {QStringLiteral("SourceGraphic"), QStringLiteral("f")});
  });
  add("flood_srgb", S::Red, [](Fixture& f) { flood(f, "#808080", 1.0); inSrgb(f); });
  add("drop_shadow_merge", S::TwoShapes, [](Fixture& f) {
    bigRegion(f);
    blur(f, 4, 4, "SourceAlpha");
    offset(f, 6, 6, "", "b");
    merge(f, {QStringLiteral("b"), QStringLiteral("SourceGraphic")});
  });
  add("glow", S::Red, [](Fixture& f) {
    blur(f, 6, 6, "SourceAlpha", "b");
    flood(f, "#ffcc00", 1.0, "c");
    Composite cp;
    cp.in2 = QStringLiteral("b");
    cp.op = QStringLiteral("in");
    Primitive& p = f.add(cp);
    p.in1 = QStringLiteral("c");
    p.result = QStringLiteral("g");
    merge(f, {QStringLiteral("g"), QStringLiteral("SourceGraphic")});
  });

  add("colormatrix_saturate", S::TwoShapes, [](Fixture& f) { colorMatrix(f, "saturate", {0.2}); });
  add("colormatrix_hue", S::TwoShapes, [](Fixture& f) { colorMatrix(f, "hueRotate", {90}); });
  add("colormatrix_l2a", S::TwoShapes, [](Fixture& f) { colorMatrix(f, "luminanceToAlpha", {}); });
  add("colormatrix_matrix", S::TwoShapes, [](Fixture& f) {
    colorMatrix(f, "matrix", {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0});
  });

  for (const char* op : {"over", "in", "out", "atop", "xor"}) {
    add((std::string("composite_") + op).c_str(), S::TwoShapes, [op](Fixture& f) {
      offset(f, 30, 20, "", "o");
      composite(f, op);
    });
  }
  add("composite_arithmetic", S::TwoShapes, [](Fixture& f) { offset(f, 30, 20, "", "o"); composite(f, "arithmetic", 0.5, 0.5, 0.5, 0); });
  add("composite_arithmetic_k4", S::TwoShapes, [](Fixture& f) { offset(f, 30, 20, "", "o"); composite(f, "arithmetic", 0, 1, -0.5, 0.1); });

  for (const char* mode : {"normal", "multiply", "screen", "darken", "lighten", "overlay", "color-dodge", "color-burn",
                           "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"}) {
    QString name = QStringLiteral("blend_") + QString(QLatin1String(mode)).replace(QLatin1Char('-'), QLatin1Char('_'));
    v.push_back({name, S::TwoShapes, [mode](Fixture& f) { blend(f, mode); }, 12, 0.0025, 0.5});
  }

  add("morph_erode_5", S::Red, [](Fixture& f) { morph(f, "erode", 5, 5); });
  add("morph_dilate_4", S::TwoShapes, [](Fixture& f) { morph(f, "dilate", 4, 4); });
  add("morph_erode_xy", S::Red, [](Fixture& f) { morph(f, "erode", 8, 2); });

  add("turbulence", S::Red, [](Fixture& f) { turbulence(f, "turbulence", 0.05, 0.05, 3, 5); });
  add("fractal_noise", S::Red, [](Fixture& f) { turbulence(f, "fractalNoise", 0.04, 0.08, 4, 2); });
  add("turbulence_srgb", S::Red, [](Fixture& f) { turbulence(f, "fractalNoise", 0.05, 0.05, 2, 1); inSrgb(f); });

  add("displacement_turbulence", S::TwoShapes, [](Fixture& f) {
    turbulence(f, "fractalNoise", 0.03, 0.03, 2, 3);
    displacement(f, 20, "R", "G");
  });
  add("displacement_alpha", S::TwoShapes, [](Fixture& f) {
    turbulence(f, "turbulence", 0.02, 0.02, 1, 7);
    displacement(f, 30, "B", "A");
  });

  add("ct_mixed", S::TwoShapes, [](Fixture& f) {
    componentTransfer(f, fn("table", {0, 0.5, 1, 0.2}), fn("discrete", {0, 0.5, 1}), fn("linear", {}, 0.5, 0.25), fn("identity"));
  });
  add("ct_gamma", S::TwoShapes, [](Fixture& f) {
    componentTransfer(f, fn("gamma", {}, 1, 0, 1.2, 2, 0), fn("gamma", {}, 1, 0, 1, 0.5, 0.1), fn("identity"), fn("identity"));
  });
  add("ct_alpha", S::TwoShapes, [](Fixture& f) {
    componentTransfer(f, fn("identity"), fn("identity"), fn("identity"), fn("linear", {}, 0.5, 0));
  });
  add("ct_srgb", S::TwoShapes, [](Fixture& f) {
    componentTransfer(f, fn("linear", {}, 0.5, 0), fn("identity"), fn("gamma", {}, 1, 0, 1, 2, 0), fn("identity"));
    inSrgb(f);
  });

  add("drop_shadow", S::TwoShapes, [](Fixture& f) {
    bigRegion(f);
    DropShadow d;
    d.dx = px(6);
    d.dy = px(6);
    d.stdDeviationX = d.stdDeviationY = 3;
    d.color = c("#000040");
    d.opacity = 0.7;
    f.add(d);
  });
  add("drop_shadow_noblur", S::TwoShapes, [](Fixture& f) {
    bigRegion(f);
    DropShadow d;
    d.dx = px(8);
    d.dy = px(-8);
    d.stdDeviationX = d.stdDeviationY = 0;
    d.color = c("#ff0000");
    f.add(d);
  });

  add("image_data", S::Red, [](Fixture& f) {
    Image im;
    im.href = QStringLiteral("data:image/png;base64,test");
    Primitive& p = f.add(im);
    p.subregion.x = px(40);
    p.subregion.y = px(40);
    p.subregion.width = px(120);
    p.subregion.height = px(120);
    f.ctx.loadImage = [](const QString&) { return testPicture(); };
  });
  add("image_element", S::Red, [](Fixture& f) {
    Image im;
    im.href = QStringLiteral("#el");
    f.add(im);
    f.ctx.renderElement = renderEl;
  });
  add("image_element_xy", S::Red, [](Fixture& f) {
    Image im;
    im.href = QStringLiteral("#el");
    Primitive& p = f.add(im);
    p.subregion.x = px(20);
    p.subregion.y = px(10);
    p.subregion.width = px(100);
    p.subregion.height = px(100);
    f.ctx.renderElement = renderEl;
  });

  add("conv_edge", S::TwoShapes, [](Fixture& f) { convolve(f, 3, 3, {0, -1, 0, -1, 4, -1, 0, -1, 0}, 1, EdgeMode::None); });
  add("conv_box", S::TwoShapes, [](Fixture& f) { convolve(f, 3, 3, {1, 1, 1, 1, 1, 1, 1, 1, 1}, 9, EdgeMode::Duplicate); });
  add("conv_sharpen_preserve", S::TwoShapes, [](Fixture& f) {
    convolve(f, 3, 3, {0, -1, 0, -1, 5, -1, 0, -1, 0}, 0, EdgeMode::Duplicate, true);
  });
  add("conv_order5", S::TwoShapes, [](Fixture& f) {
    convolve(f, 5, 5, {1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4, 1}, 0, EdgeMode::None);
  });
  add("conv_target", S::TwoShapes, [](Fixture& f) { convolve(f, 3, 3, {1, 0, 0, 0, 0, 0, 0, 0, 0}, 1, EdgeMode::None, false, 0, 0); });

  add("tile", S::TwoShapes, [](Fixture& f) {
    offset(f, 10, 10, "SourceGraphic", "t");
    Primitive& o = f.store.back();
    o.subregion.x = px(50);
    o.subregion.y = px(50);
    o.subregion.width = px(40);
    o.subregion.height = px(40);
    f.add(Tile{}).in1 = QStringLiteral("t");
  });

  add("diffuse_distant", S::Bump, [](Fixture& f) { diffuse(f, "#ffffff", distant()); });
  // resvg does not convert lighting-color into the working space (a mid gray
  // lighting color comes out as 188 in the default linearRGB), where browsers
  // and this engine do. The chromatic lighting goldens therefore run in sRGB,
  // where the two agree; LightingColorIsConvertedToWorkingSpace covers linearRGB.
  add("diffuse_point", S::Bump, [](Fixture& f) { diffuse(f, "#ffe0c0", point()); inSrgb(f); });
  add("diffuse_spot", S::Bump, [](Fixture& f) { diffuse(f, "#ffffff", spot(4, 40)); });
  add("specular_distant", S::Bump, [](Fixture& f) { specular(f, "#ff8000", 1, 20, distant()); inSrgb(f); });
  add("specular_point", S::Bump, [](Fixture& f) { specular(f, "#40a0ff", 1.2, 10, point()); inSrgb(f); });
  add("specular_spot", S::Bump, [](Fixture& f) { specular(f, "#c000c0", 1, 8, spot(3, 50)); inSrgb(f); });

  // Looser limits, each with its cause. Everything not listed keeps the
  // defaults (12 levels, 0.25% of pixels, mean 0.5).
  struct Loosen {
    const char* name;
    double over, mean;
    const char* why;
  };
  const Loosen loosen[] = {
      {"blur_box_5", 0.0025, 1.0, "three box passes here, five in resvg: the tails differ by a few levels"},
      {"blur_xy", 0.0025, 0.9, "same box approximation"},
      {"blur_srgb", 0.0025, 0.6, "same box approximation"},
      {"blur_kernel_1", 0.016, 0.8, "sampled kernel here, IIR filter in resvg: the edge ramps differ by up to 17 levels"},
      {"morph_erode_5", 0.0025, 0.6, "resvg round-trips the shape through 8-bit linearRGB (48 becomes 50); this engine does not convert"},
      {"morph_erode_xy", 0.0025, 0.6, "same round trip"},
      {"morph_dilate_4", 0.0025, 0.6, "same round trip"},
      {"blend_luminosity", 0.0025, 0.8, "8-bit linearRGB quantisation in resvg, amplified by the luminosity mix"},
      {"diffuse_spot", 0.006, 0.5, "the blurred bump differs slightly (box passes) and the steep crescent edge amplifies it"},
      {"specular_distant", 0.02, 0.6, "blurred bump differs slightly (box passes), the exponent 20 highlight amplifies it"},
      {"specular_point", 0.0025, 0.5, "same bump difference"},
      {"conv_sharpen_preserve", 0.005, 0.5, "sharpening multiplies the anti-aliasing noise on shape edges by up to 5"},
      {"conv_edge", 0.003, 0.5, "edge detection reacts to anti-aliasing differences along the outline"},
  };
  for (const Loosen& l : loosen) {
    for (GoldenCase& g : v) {
      if (g.name == QLatin1String(l.name)) {
        g.maxOver = l.over;
        g.maxMean = l.mean;
      }
    }
  }
  return v;
}

}  // namespace filtertest
