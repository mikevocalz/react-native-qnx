#include "react-native-qnx/views/image/BlurHashEncoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rnqnx::views {
namespace {

constexpr char kAlphabet[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~";
constexpr int kMaxLongSide = 100;

using Factor = std::array<double, 3>;

void append83(QString& out, int value, int length) {
  for (int i = 1; i <= length; ++i) {
    int divisor = 1;
    for (int j = 0; j < length - i; ++j) divisor *= 83;
    out.append(QLatin1Char(kAlphabet[(value / divisor) % 83]));
  }
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

Factor basisFactor(const QImage& rgba, int cx, int cy) {
  const int width = rgba.width();
  const int height = rgba.height();
  const double normalisation = cx == 0 && cy == 0 ? 1.0 : 2.0;
  Factor sum{0, 0, 0};
  for (int y = 0; y < height; ++y) {
    const uchar* row = rgba.constScanLine(y);
    for (int x = 0; x < width; ++x) {
      const double basis = normalisation * std::cos(M_PI * cx * x / width) * std::cos(M_PI * cy * y / height);
      for (size_t c = 0; c < 3; ++c) sum[c] += basis * srgbToLinear(row[4 * x + static_cast<int>(c)]);
    }
  }
  const double scale = 1.0 / (static_cast<double>(width) * height);
  for (double& channel : sum) channel *= scale;
  return sum;
}

int encodeDc(const Factor& value) {
  return (linearToSrgb(value[0]) << 16) + (linearToSrgb(value[1]) << 8) + linearToSrgb(value[2]);
}

int quantiseAc(double value, double maximum) {
  return static_cast<int>(std::clamp(std::floor(signPow(value / maximum, 0.5) * 9 + 9.5), 0.0, 18.0));
}

int encodeAc(const Factor& value, double maximum) {
  return quantiseAc(value[0], maximum) * 19 * 19 + quantiseAc(value[1], maximum) * 19 +
         quantiseAc(value[2], maximum);
}

}  // namespace

QString encodeBlurHash(const QImage& image, int componentsX, int componentsY) {
  if (image.isNull() || componentsX < 1 || componentsX > 9 || componentsY < 1 || componentsY > 9) return {};
  QImage rgba = image;
  if (std::max(rgba.width(), rgba.height()) > kMaxLongSide) {
    rgba = rgba.scaled(kMaxLongSide, kMaxLongSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  rgba = rgba.convertToFormat(QImage::Format_RGBA8888);

  std::vector<Factor> factors;
  factors.reserve(static_cast<size_t>(componentsX * componentsY));
  for (int cy = 0; cy < componentsY; ++cy) {
    for (int cx = 0; cx < componentsX; ++cx) factors.push_back(basisFactor(rgba, cx, cy));
  }

  QString hash;
  append83(hash, (componentsX - 1) + (componentsY - 1) * 9, 1);
  double maximum = 1.0;
  if (factors.size() > 1) {
    double peak = 0;
    for (size_t i = 1; i < factors.size(); ++i) {
      for (double channel : factors[i]) peak = std::max(peak, channel);
    }
    const int quantised = static_cast<int>(std::clamp(std::floor(peak * 166 - 0.5), 0.0, 82.0));
    maximum = (quantised + 1) / 166.0;
    append83(hash, quantised, 1);
  } else {
    append83(hash, 0, 1);
  }
  append83(hash, encodeDc(factors[0]), 4);
  for (size_t i = 1; i < factors.size(); ++i) append83(hash, encodeAc(factors[i], maximum), 2);
  return hash;
}

}  // namespace rnqnx::views
