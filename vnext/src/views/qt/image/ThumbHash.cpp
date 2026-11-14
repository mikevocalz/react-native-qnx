#include "react-native-qnx/views/image/ThumbHash.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rnqnx::views {
namespace {

// The AC coefficients of one channel, in the triangular order ThumbHash uses.
std::vector<double> readChannel(const QByteArray& hash, int start, int& index, int nx, int ny,
                                double scale) {
  std::vector<double> ac;
  for (int cy = 0; cy < ny; ++cy) {
    for (int cx = cy ? 0 : 1; cx * ny < nx * (ny - cy); ++cx) {
      const auto byte = static_cast<unsigned char>(hash.at(start + (index >> 1)));
      const int nibble = (byte >> ((index & 1) << 2)) & 15;
      ++index;
      ac.push_back((nibble / 7.5 - 1) * scale);
    }
  }
  return ac;
}

}  // namespace

QImage decodeThumbHash(const QString& base64) {
  return decodeThumbHash(QByteArray::fromBase64(base64.toLatin1()));
}

QImage decodeThumbHash(const QByteArray& hash) {
  if (hash.size() < 5) return {};
  const auto byte = [&](int i) { return static_cast<unsigned char>(hash.at(i)); };
  const int header24 = byte(0) | (byte(1) << 8) | (byte(2) << 16);
  const int header16 = byte(3) | (byte(4) << 8);
  const double lDc = (header24 & 63) / 63.0;
  const double pDc = ((header24 >> 6) & 63) / 31.5 - 1;
  const double qDc = ((header24 >> 12) & 63) / 31.5 - 1;
  const double lScale = ((header24 >> 18) & 31) / 31.0;
  const bool hasAlpha = (header24 >> 23) != 0;
  const double pScale = ((header16 >> 3) & 63) / 63.0;
  const double qScale = ((header16 >> 9) & 63) / 63.0;
  const bool landscape = (header16 >> 15) != 0;
  const int lx = std::max(3, landscape ? (hasAlpha ? 5 : 7) : header16 & 7);
  const int ly = std::max(3, landscape ? header16 & 7 : (hasAlpha ? 5 : 7));
  const int acStart = hasAlpha ? 6 : 5;
  if (hasAlpha && hash.size() < 6) return {};
  const double aDc = hasAlpha ? (byte(5) & 15) / 15.0 : 1.0;
  const double aScale = hasAlpha ? (byte(5) >> 4) / 15.0 : 0.0;

  // Count the AC nibbles to reject a truncated hash before reading it.
  const auto count = [](int nx, int ny) {
    int n = 0;
    for (int cy = 0; cy < ny; ++cy)
      for (int cx = cy ? 0 : 1; cx * ny < nx * (ny - cy); ++cx) ++n;
    return n;
  };
  const int nibbles = count(lx, ly) + 2 * count(3, 3) + (hasAlpha ? count(5, 5) : 0);
  if (hash.size() < acStart + (nibbles + 1) / 2) return {};

  int index = 0;
  const auto lAc = readChannel(hash, acStart, index, lx, ly, lScale);
  const auto pAc = readChannel(hash, acStart, index, 3, 3, pScale * 1.25);
  const auto qAc = readChannel(hash, acStart, index, 3, 3, qScale * 1.25);
  std::vector<double> aAc;
  if (hasAlpha) aAc = readChannel(hash, acStart, index, 5, 5, aScale);

  const double ratio = static_cast<double>(landscape ? (hasAlpha ? 5 : 7) : header16 & 7) /
                       static_cast<double>(landscape ? header16 & 7 : (hasAlpha ? 5 : 7));
  const int w = std::max(1, static_cast<int>(std::lround(ratio > 1 ? 32 : 32 * ratio)));
  const int h = std::max(1, static_cast<int>(std::lround(ratio > 1 ? 32 / ratio : 32)));

  QImage image(w, h, QImage::Format_ARGB32);
  std::vector<double> fx(static_cast<size_t>(std::max(lx, hasAlpha ? 5 : 3)));
  std::vector<double> fy(static_cast<size_t>(std::max(ly, hasAlpha ? 5 : 3)));
  for (int y = 0; y < h; ++y) {
    auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < w; ++x) {
      for (size_t cx = 0; cx < fx.size(); ++cx) fx[cx] = std::cos(M_PI / w * (x + 0.5) * static_cast<double>(cx));
      for (size_t cy = 0; cy < fy.size(); ++cy) fy[cy] = std::cos(M_PI / h * (y + 0.5) * static_cast<double>(cy));
      double l = lDc, p = pDc, q = qDc, a = aDc;
      size_t j = 0;
      for (int cy = 0; cy < ly; ++cy) {
        const double fy2 = fy[static_cast<size_t>(cy)] * 2;
        for (int cx = cy ? 0 : 1; cx * ly < lx * (ly - cy); ++cx, ++j) {
          l += lAc[j] * fx[static_cast<size_t>(cx)] * fy2;
        }
      }
      j = 0;
      for (int cy = 0; cy < 3; ++cy) {
        const double fy2 = fy[static_cast<size_t>(cy)] * 2;
        for (int cx = cy ? 0 : 1; cx < 3 - cy; ++cx, ++j) {
          const double f = fx[static_cast<size_t>(cx)] * fy2;
          p += pAc[j] * f;
          q += qAc[j] * f;
        }
      }
      if (hasAlpha) {
        j = 0;
        for (int cy = 0; cy < 5; ++cy) {
          const double fy2 = fy[static_cast<size_t>(cy)] * 2;
          for (int cx = cy ? 0 : 1; cx < 5 - cy; ++cx, ++j) {
            a += aAc[j] * fx[static_cast<size_t>(cx)] * fy2;
          }
        }
      }
      const double b = l - 2.0 / 3.0 * p;
      const double r = (3 * l - b + q) / 2;
      const double g = r - q;
      const auto channel = [](double v) { return static_cast<int>(std::max(0.0, 255 * std::min(1.0, v))); };
      row[x] = qRgba(channel(r), channel(g), channel(b), channel(a));
    }
  }
  return image;
}

}  // namespace rnqnx::views
