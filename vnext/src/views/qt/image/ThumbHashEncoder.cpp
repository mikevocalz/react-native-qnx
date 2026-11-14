#include "react-native-qnx/views/image/ThumbHashEncoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rnqnx::views {
namespace {

constexpr int kMaxSide = 100;

// JavaScript's Math.round: halves go up.
int jsRound(double value) { return static_cast<int>(std::floor(value + 0.5)); }

struct Channel {
  double dc{0};
  std::vector<double> ac;
  double scale{0};
};

// The DCT of one channel into a constant term and normalized varying terms, in
// the triangular order the decoder reads.
Channel encodeChannel(const std::vector<double>& channel, int w, int h, int nx, int ny) {
  Channel out;
  std::vector<double> fx(static_cast<size_t>(w));
  for (int cy = 0; cy < ny; ++cy) {
    for (int cx = 0; cx * ny < nx * (ny - cy); ++cx) {
      for (int x = 0; x < w; ++x) fx[static_cast<size_t>(x)] = std::cos(M_PI / w * cx * (x + 0.5));
      double f = 0;
      for (int y = 0; y < h; ++y) {
        const double fy = std::cos(M_PI / h * cy * (y + 0.5));
        for (int x = 0; x < w; ++x) {
          f += channel[static_cast<size_t>(x + y * w)] * fx[static_cast<size_t>(x)] * fy;
        }
      }
      f /= static_cast<double>(w) * h;
      if (cx != 0 || cy != 0) {
        out.ac.push_back(f);
        out.scale = std::max(out.scale, std::fabs(f));
      } else {
        out.dc = f;
      }
    }
  }
  if (out.scale > 0) {
    for (double& value : out.ac) value = 0.5 + 0.5 / out.scale * value;
  }
  return out;
}

}  // namespace

QByteArray encodeThumbHash(const QImage& image) {
  if (image.isNull()) return {};
  QImage source = image;
  if (source.width() > kMaxSide || source.height() > kMaxSide) {
    source = source.scaled(kMaxSide, kMaxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
  const int w = rgba.width();
  const int h = rgba.height();
  const auto pixel = [&](int i, int channel) {
    return static_cast<double>(rgba.constScanLine(i / w)[4 * (i % w) + channel]);
  };

  double avgR = 0, avgG = 0, avgB = 0, avgA = 0;
  for (int i = 0; i < w * h; ++i) {
    const double alpha = pixel(i, 3) / 255;
    avgR += alpha / 255 * pixel(i, 0);
    avgG += alpha / 255 * pixel(i, 1);
    avgB += alpha / 255 * pixel(i, 2);
    avgA += alpha;
  }
  if (avgA > 0) {
    avgR /= avgA;
    avgG /= avgA;
    avgB /= avgA;
  }

  const bool hasAlpha = avgA < static_cast<double>(w) * h;
  const int lLimit = hasAlpha ? 5 : 7;
  const int longSide = std::max(w, h);
  const int lx = std::max(1, jsRound(static_cast<double>(lLimit) * w / longSide));
  const int ly = std::max(1, jsRound(static_cast<double>(lLimit) * h / longSide));

  const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<double> l(count), p(count), q(count), a(count);
  for (int i = 0; i < w * h; ++i) {
    const double alpha = pixel(i, 3) / 255;
    const double r = avgR * (1 - alpha) + alpha / 255 * pixel(i, 0);
    const double g = avgG * (1 - alpha) + alpha / 255 * pixel(i, 1);
    const double b = avgB * (1 - alpha) + alpha / 255 * pixel(i, 2);
    const auto at = static_cast<size_t>(i);
    l[at] = (r + g + b) / 3;
    p[at] = (r + g) / 2 - b;
    q[at] = r - g;
    a[at] = alpha;
  }

  const Channel lChannel = encodeChannel(l, w, h, std::max(3, lx), std::max(3, ly));
  const Channel pChannel = encodeChannel(p, w, h, 3, 3);
  const Channel qChannel = encodeChannel(q, w, h, 3, 3);
  Channel aChannel;
  if (hasAlpha) aChannel = encodeChannel(a, w, h, 5, 5);

  const bool landscape = w > h;
  const int header24 = jsRound(63 * lChannel.dc) | (jsRound(31.5 + 31.5 * pChannel.dc) << 6) |
                       (jsRound(31.5 + 31.5 * qChannel.dc) << 12) | (jsRound(31 * lChannel.scale) << 18) |
                       ((hasAlpha ? 1 : 0) << 23);
  const int header16 = (landscape ? ly : lx) | (jsRound(63 * pChannel.scale) << 3) |
                       (jsRound(63 * qChannel.scale) << 9) | ((landscape ? 1 : 0) << 15);
  QByteArray hash;
  hash.append(static_cast<char>(header24 & 255));
  hash.append(static_cast<char>((header24 >> 8) & 255));
  hash.append(static_cast<char>(header24 >> 16));
  hash.append(static_cast<char>(header16 & 255));
  hash.append(static_cast<char>(header16 >> 8));
  const int acStart = hasAlpha ? 6 : 5;
  if (hasAlpha) hash.append(static_cast<char>(jsRound(15 * aChannel.dc) | (jsRound(15 * aChannel.scale) << 4)));

  int acIndex = 0;
  const auto writeAc = [&](const Channel& channel) {
    for (double f : channel.ac) {
      const int position = acStart + (acIndex >> 1);
      if (hash.size() <= position) hash.resize(position + 1, 0);
      hash[position] = static_cast<char>(static_cast<unsigned char>(hash.at(position)) |
                                         (jsRound(15 * f) << ((acIndex & 1) << 2)));
      ++acIndex;
    }
  };
  writeAc(lChannel);
  writeAc(pChannel);
  writeAc(qChannel);
  if (hasAlpha) writeAc(aChannel);
  return hash;
}

}  // namespace rnqnx::views
