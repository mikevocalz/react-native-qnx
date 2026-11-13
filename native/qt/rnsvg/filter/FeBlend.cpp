#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// W3C Compositing and Blending Level 1. `cb` is the backdrop (in2) and `cs`
// the source (in), both as non-premultiplied 0..1 values.
struct Rgb {
  float r, g, b;
};

float lum(Rgb c) { return 0.3f * c.r + 0.59f * c.g + 0.11f * c.b; }

Rgb clipColor(Rgb c) {
  const float l = lum(c);
  const float n = std::min({c.r, c.g, c.b}), x = std::max({c.r, c.g, c.b});
  if (n < 0) {
    const float d = l - n;
    c = {l + (c.r - l) * l / d, l + (c.g - l) * l / d, l + (c.b - l) * l / d};
  }
  if (x > 1) {
    const float d = x - l;
    c = {l + (c.r - l) * (1 - l) / d, l + (c.g - l) * (1 - l) / d, l + (c.b - l) * (1 - l) / d};
  }
  return c;
}

Rgb setLum(Rgb c, float l) {
  const float d = l - lum(c);
  return clipColor({c.r + d, c.g + d, c.b + d});
}

float sat(Rgb c) { return std::max({c.r, c.g, c.b}) - std::min({c.r, c.g, c.b}); }

Rgb setSat(Rgb c, float s) {
  float* v[3] = {&c.r, &c.g, &c.b};
  std::sort(v, v + 3, [](const float* a, const float* b) { return *a < *b; });
  const float mn = *v[0], mx = *v[2];
  if (mx > mn) {
    *v[1] = (*v[1] - mn) * s / (mx - mn);
    *v[2] = s;
  } else {
    *v[1] = *v[2] = 0;
  }
  *v[0] = 0;
  return c;
}

float hardLight(float cb, float cs) { return cs <= 0.5f ? cb * 2 * cs : cb + (2 * cs - 1) - cb * (2 * cs - 1); }

float softLight(float cb, float cs) {
  if (cs <= 0.5f) return cb - (1 - 2 * cs) * cb * (1 - cb);
  const float d = cb <= 0.25f ? ((16 * cb - 12) * cb + 4) * cb : std::sqrt(cb);
  return cb + (2 * cs - 1) * (d - cb);
}

float colorDodge(float cb, float cs) {
  if (cb == 0) return 0;
  if (cs >= 1) return 1;
  return std::min(1.0f, cb / (1 - cs));
}

float colorBurn(float cb, float cs) {
  if (cb >= 1) return 1;
  if (cs <= 0) return 0;
  return 1 - std::min(1.0f, (1 - cb) / cs);
}

// Runs the per-pixel loop with the blend function `fn(Rgb backdrop, Rgb
// source) -> Rgb` inlined, so the mode switch is outside the loop.
template <class F>
void run(QImage& out, const QImage& src, const QImage& back, F fn) {
  const float k = 1.0f / 255.0f;
  for (int y = 0; y < out.height(); ++y) {
    const auto* ps = reinterpret_cast<const QRgb*>(src.constScanLine(y));
    const auto* pb = reinterpret_cast<const QRgb*>(back.constScanLine(y));
    auto* po = reinterpret_cast<QRgb*>(out.scanLine(y));
    for (int x = 0; x < out.width(); ++x) {
      const QRgb s = ps[x], b = pb[x];
      const int sa = qAlpha(s), ba = qAlpha(b);
      if (ba == 0) { po[x] = s; continue; }
      if (sa == 0) { po[x] = b; continue; }
      const float fsa = sa * k, fba = ba * k;
      const Rgb cs{demul(qRed(s), sa) * k, demul(qGreen(s), sa) * k, demul(qBlue(s), sa) * k};
      const Rgb cb{demul(qRed(b), ba) * k, demul(qGreen(b), ba) * k, demul(qBlue(b), ba) * k};
      const Rgb m = fn(cb, cs);
      // Premultiplied result: (1-ab)*cs*as + (1-as)*cb*ab + as*ab*B.
      const float wS = fsa * (1 - fba), wB = fba * (1 - fsa), wM = fsa * fba;
      auto ch = [&](float s1, float b1, float m1) {
        return clampByte(static_cast<int>((wS * s1 + wB * b1 + wM * m1) * 255.0f + 0.5f));
      };
      const int ao = clampByte(static_cast<int>((fsa + fba - fsa * fba) * 255.0f + 0.5f));
      po[x] = qRgba(std::min(ao, ch(cs.r, cb.r, m.r)), std::min(ao, ch(cs.g, cb.g, m.g)),
                    std::min(ao, ch(cs.b, cb.b, m.b)), ao);
    }
  }
}

template <class F>
void runSeparable(QImage& out, const QImage& src, const QImage& back, F fn) {
  run(out, src, back, [&](Rgb cb, Rgb cs) { return Rgb{fn(cb.r, cs.r), fn(cb.g, cs.g), fn(cb.b, cs.b)}; });
}

}  // namespace

Layer applyBlend(const Env&, const Blend& p, ColorSpace cs, const QRect&, const Layer& in, const Layer& in2) {
  const QImage src = inColorSpace(in.image, in.space, cs);
  const QImage back = inColorSpace(in2.image, in2.space, cs);
  QImage out(src.size(), QImage::Format_ARGB32_Premultiplied);
  const QString& m = p.mode;
  if (m == QLatin1String("multiply")) runSeparable(out, src, back, [](float b, float s) { return b * s; });
  else if (m == QLatin1String("screen")) runSeparable(out, src, back, [](float b, float s) { return b + s - b * s; });
  else if (m == QLatin1String("darken")) runSeparable(out, src, back, [](float b, float s) { return std::min(b, s); });
  else if (m == QLatin1String("lighten")) runSeparable(out, src, back, [](float b, float s) { return std::max(b, s); });
  else if (m == QLatin1String("overlay")) runSeparable(out, src, back, [](float b, float s) { return hardLight(s, b); });
  else if (m == QLatin1String("color-dodge")) runSeparable(out, src, back, colorDodge);
  else if (m == QLatin1String("color-burn")) runSeparable(out, src, back, colorBurn);
  else if (m == QLatin1String("hard-light")) runSeparable(out, src, back, hardLight);
  else if (m == QLatin1String("soft-light")) runSeparable(out, src, back, softLight);
  else if (m == QLatin1String("difference")) runSeparable(out, src, back, [](float b, float s) { return std::abs(b - s); });
  else if (m == QLatin1String("exclusion")) runSeparable(out, src, back, [](float b, float s) { return b + s - 2 * b * s; });
  else if (m == QLatin1String("hue")) run(out, src, back, [](Rgb b, Rgb s) { return setLum(setSat(s, sat(b)), lum(b)); });
  else if (m == QLatin1String("saturation")) run(out, src, back, [](Rgb b, Rgb s) { return setLum(setSat(b, sat(s)), lum(b)); });
  else if (m == QLatin1String("color")) run(out, src, back, [](Rgb b, Rgb s) { return setLum(s, lum(b)); });
  else if (m == QLatin1String("luminosity")) run(out, src, back, [](Rgb b, Rgb s) { return setLum(b, lum(s)); });
  else runSeparable(out, src, back, [](float, float s) { return s; });  // normal, and anything unrecognised
  return {out, {}, cs};
}

}  // namespace rnqnx::views::rnsvg::filter::detail
