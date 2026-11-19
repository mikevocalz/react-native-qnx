#include "FilterColorSpace.h"
#include "FilterPrimitives.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::filter::detail {
namespace {

// The reference implementation from the SVG 1.1 specification (section
// "feTurbulence"), so seed and output match other renderers that port it.

constexpr int kRandM = 2147483647;
constexpr int kRandA = 16807;
constexpr int kRandQ = 127773;
constexpr int kRandR = 2836;
constexpr int kBSize = 0x100;
constexpr int kBM = 0xff;
constexpr int kPerlinN = 0x1000;

// 64-bit so that doubling the values once per octave cannot overflow.
struct StitchInfo {
  long long width, height, wrapX, wrapY;
};

long setupSeed(long seed) {
  if (seed <= 0) seed = -(seed % (kRandM - 1)) + 1;
  if (seed > kRandM - 1) seed = kRandM - 1;
  return seed;
}

long nextRandom(long seed) {
  long result = kRandA * (seed % kRandQ) - kRandR * (seed / kRandQ);
  if (result <= 0) result += kRandM;
  return result;
}

class Noise {
 public:
  explicit Noise(long seed) {
    seed = setupSeed(seed);
    int i = 0;
    for (int k = 0; k < 4; ++k) {
      for (i = 0; i < kBSize; ++i) {
        lattice_[i] = i;
        for (int j = 0; j < 2; ++j) {
          seed = nextRandom(seed);
          gradient_[k][i][j] = static_cast<double>((seed % (kBSize + kBSize)) - kBSize) / kBSize;
        }
        const double s = std::sqrt(gradient_[k][i][0] * gradient_[k][i][0] + gradient_[k][i][1] * gradient_[k][i][1]);
        if (s > 0) {
          gradient_[k][i][0] /= s;
          gradient_[k][i][1] /= s;
        }
      }
    }
    while (--i) {
      const int k = lattice_[i];
      seed = nextRandom(seed);
      const int j = static_cast<int>(seed % kBSize);
      lattice_[i] = lattice_[j];
      lattice_[j] = k;
    }
    for (i = 0; i < kBSize + 2; ++i) {
      lattice_[kBSize + i] = lattice_[i];
      for (int k = 0; k < 4; ++k) {
        for (int j = 0; j < 2; ++j) gradient_[k][kBSize + i][j] = gradient_[k][i][j];
      }
    }
  }

  double turbulence(int channel, double px, double py, double baseX, double baseY, int octaves, bool fractal,
                    bool stitch, double tileX, double tileY, double tileW, double tileH) const {
    StitchInfo info{};
    StitchInfo* pInfo = nullptr;
    if (stitch) {
      // Adjust the frequencies so tile borders are continuous.
      if (baseX != 0.0) {
        const double lo = std::floor(tileW * baseX) / tileW, hi = std::ceil(tileW * baseX) / tileW;
        baseX = (baseX / lo < hi / baseX) ? lo : hi;
      }
      if (baseY != 0.0) {
        const double lo = std::floor(tileH * baseY) / tileH, hi = std::ceil(tileH * baseY) / tileH;
        baseY = (baseY / lo < hi / baseY) ? lo : hi;
      }
      auto toInt = [](double v) { return static_cast<long long>(std::clamp(std::isfinite(v) ? v : 0.0, -1.0e9, 1.0e9)); };
      info.width = toInt(tileW * baseX + 0.5);
      info.wrapX = toInt(tileX * baseX + kPerlinN + info.width);
      info.height = toInt(tileH * baseY + 0.5);
      info.wrapY = toInt(tileY * baseY + kPerlinN + info.height);
      pInfo = &info;
    }
    double sum = 0;
    double v0 = px * baseX, v1 = py * baseY;
    double ratio = 1;
    for (int o = 0; o < octaves; ++o) {
      const double n = noise2(channel, v0, v1, pInfo);
      sum += (fractal ? n : std::fabs(n)) / ratio;
      v0 *= 2;
      v1 *= 2;
      ratio *= 2;
      if (pInfo) {
        pInfo->width *= 2;
        pInfo->wrapX = 2 * pInfo->wrapX - kPerlinN;
        pInfo->height *= 2;
        pInfo->wrapY = 2 * pInfo->wrapY - kPerlinN;
      }
    }
    return sum;
  }

 private:
  static double sCurve(double t) { return t * t * (3.0 - 2.0 * t); }
  static double lerp(double t, double a, double b) { return a + t * (b - a); }

  double noise2(int channel, double v0, double v1, const StitchInfo* st) const {
    if (!std::isfinite(v0) || !std::isfinite(v1)) return 0;
    double t = std::clamp(v0 + kPerlinN, -1.0e9, 1.0e9);  // keeps the int casts defined
    long long bx0 = static_cast<long long>(t), bx1 = bx0 + 1;
    const double rx0 = t - static_cast<long>(t), rx1 = rx0 - 1.0;
    t = std::clamp(v1 + kPerlinN, -1.0e9, 1.0e9);
    long long by0 = static_cast<long long>(t), by1 = by0 + 1;
    const double ry0 = t - static_cast<long>(t), ry1 = ry0 - 1.0;
    if (st) {  // keep lattice points continuous across the tile border
      if (bx0 >= st->wrapX) bx0 -= st->width;
      if (bx1 >= st->wrapX) bx1 -= st->width;
      if (by0 >= st->wrapY) by0 -= st->height;
      if (by1 >= st->wrapY) by1 -= st->height;
    }
    const int ix0 = static_cast<int>(bx0 & kBM), ix1 = static_cast<int>(bx1 & kBM);
    const int iy0 = static_cast<int>(by0 & kBM), iy1 = static_cast<int>(by1 & kBM);
    const int i = lattice_[ix0], j = lattice_[ix1];
    const int b00 = lattice_[i + iy0], b10 = lattice_[j + iy0], b01 = lattice_[i + iy1], b11 = lattice_[j + iy1];
    const double sx = sCurve(rx0), sy = sCurve(ry0);
    const double* q = gradient_[channel][b00];
    double u = rx0 * q[0] + ry0 * q[1];
    q = gradient_[channel][b10];
    double v = rx1 * q[0] + ry0 * q[1];
    const double a = lerp(sx, u, v);
    q = gradient_[channel][b01];
    u = rx0 * q[0] + ry1 * q[1];
    q = gradient_[channel][b11];
    v = rx1 * q[0] + ry1 * q[1];
    const double b = lerp(sx, u, v);
    return lerp(sy, a, b);
  }

  int lattice_[kBSize + kBSize + 2];
  double gradient_[4][kBSize + kBSize + 2][2];
};

}  // namespace

// baseFrequency below zero or not finite gives a transparent result (an error
// in the specification). numOctaves is capped at kMaxOctaves and a negative
// count is 0. The seed is truncated toward zero, as the specification says.
// Noise values are written in the primitive's color space, so the layer is
// tagged with it. stitchTiles uses the primitive subregion in user space as
// the tile.
Layer applyTurbulence(const Env& env, const Turbulence& p, ColorSpace cs, const QRect& sub,
                      const QRectF& userSub) {
  Layer out{makeImage(env.region.size()), {}, cs};
  const double fx = p.baseFrequencyX, fy = p.baseFrequencyY;
  if (!std::isfinite(fx) || !std::isfinite(fy) || fx < 0 || fy < 0) return out;
  const int octaves = std::clamp(p.numOctaves, 0, kMaxOctaves);
  const double seedValue = std::isfinite(p.seed) ? std::trunc(p.seed) : 0.0;
  const Noise noise(static_cast<long>(std::clamp(seedValue, -2147483646.0, 2147483646.0)));
  const bool fractal = p.type == QLatin1String("fractalNoise");
  const bool stitch = p.stitchTiles && userSub.width() > 0 && userSub.height() > 0;
  const QRect r = sub.intersected(out.image.rect());
  for (int y = r.top(); y <= r.bottom(); ++y) {
    auto* line = reinterpret_cast<QRgb*>(out.image.scanLine(y));
    for (int x = r.left(); x <= r.right(); ++x) {
      const QPointF u = env.fromDevice.map(QPointF(env.region.x() + x, env.region.y() + y));
      int c[4];
      for (int k = 0; k < 4; ++k) {
        double sum = noise.turbulence(k, u.x(), u.y(), fx, fy, octaves, fractal, stitch, userSub.x(), userSub.y(),
                                      userSub.width(), userSub.height());
        sum = fractal ? (sum * 255.0 + 255.0) / 2.0 : sum * 255.0;
        c[k] = std::isfinite(sum) ? clampByte(static_cast<int>(std::lround(sum))) : 0;
      }
      line[x] = qRgba(premul(c[0], c[3]), premul(c[1], c[3]), premul(c[2], c[3]), c[3]);
    }
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::filter::detail
