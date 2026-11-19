#include "react-native-qnx/views/image/BlurHash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rnqnx::views {
namespace {

constexpr char kAlphabet[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~";

int digit(QChar c) {
  const char latin = c.toLatin1();
  if (latin == 0) return -1;
  for (int i = 0; i < 83; ++i) {
    if (kAlphabet[i] == latin) return i;
  }
  return -1;
}

bool decode83(const QString& text, int from, int length, int& out) {
  out = 0;
  for (int i = 0; i < length; ++i) {
    const int d = digit(text.at(from + i));
    if (d < 0) return false;
    out = out * 83 + d;
  }
  return true;
}

double srgbToLinear(int value) {
  const double v = value / 255.0;
  return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}
int linearToSrgb(double value) {
  const double v = std::clamp(value, 0.0, 1.0);
  return static_cast<int>(v <= 0.0031308 ? v * 12.92 * 255 + 0.5
                                         : (1.055 * std::pow(v, 1 / 2.4) - 0.055) * 255 + 0.5);
}
double signPow(double value, double exponent) {
  return std::copysign(std::pow(std::fabs(value), exponent), value);
}

using Color = std::array<double, 3>;

}  // namespace

QImage decodeBlurHash(const QString& hash, int width, int height) {
  if (width <= 0 || height <= 0 || hash.size() < 6) return {};
  int sizeFlag = 0;
  if (!decode83(hash, 0, 1, sizeFlag)) return {};
  const int componentsY = sizeFlag / 9 + 1;
  const int componentsX = sizeFlag % 9 + 1;
  if (hash.size() != 4 + 2 * componentsX * componentsY) return {};
  int quantMax = 0;
  if (!decode83(hash, 1, 1, quantMax)) return {};
  const double maximum = (quantMax + 1) / 166.0;

  std::vector<Color> colors(static_cast<size_t>(componentsX * componentsY));
  int dc = 0;
  if (!decode83(hash, 2, 4, dc)) return {};
  colors[0] = {srgbToLinear(dc >> 16), srgbToLinear((dc >> 8) & 255), srgbToLinear(dc & 255)};
  for (int i = 1; i < componentsX * componentsY; ++i) {
    int ac = 0;
    if (!decode83(hash, 4 + i * 2, 2, ac)) return {};
    const auto channel = [&](int q) { return signPow((q - 9.0) / 9.0, 2.0) * maximum; };
    colors[static_cast<size_t>(i)] = {channel(ac / (19 * 19)), channel((ac / 19) % 19), channel(ac % 19)};
  }

  QImage image(width, height, QImage::Format_RGB32);
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < width; ++x) {
      Color sum{0, 0, 0};
      for (int j = 0; j < componentsY; ++j) {
        for (int i = 0; i < componentsX; ++i) {
          const double basis = std::cos(M_PI * x * i / width) * std::cos(M_PI * y * j / height);
          const Color& color = colors[static_cast<size_t>(i + j * componentsX)];
          for (int c = 0; c < 3; ++c) sum[static_cast<size_t>(c)] += color[static_cast<size_t>(c)] * basis;
        }
      }
      row[x] = qRgb(linearToSrgb(sum[0]), linearToSrgb(sum[1]), linearToSrgb(sum[2]));
    }
  }
  return image;
}

}  // namespace rnqnx::views
