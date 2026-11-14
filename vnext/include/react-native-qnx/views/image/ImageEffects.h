#pragma once

#include <QColor>
#include <QImage>

namespace rnqnx::views {

/// A box blur of three passes (close to a Gaussian) with \a radius pixels per
/// pass side. Returns a premultiplied ARGB copy; radius <= 0 returns the input.
QImage blurImage(const QImage& source, int radius);

/// Replaces the color of every pixel with \a color, keeping its alpha (the
/// `tintColor` of expo-image).
QImage tintImage(const QImage& source, const QColor& color);

}  // namespace rnqnx::views
