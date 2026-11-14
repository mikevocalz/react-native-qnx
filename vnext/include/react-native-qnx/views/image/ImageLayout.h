#pragma once

#include <QPainterPath>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVariant>

#include <array>

namespace rnqnx::views {

enum class ContentFit { Cover, Contain, Fill, None, ScaleDown };

/// Unknown text is `cover`, expo-image's default.
ContentFit parseContentFit(const QString& text);

/// One axis of expo-image's contentPosition. A percentage is of the free space
/// (view minus drawn image), counted from the start edge, or from the end edge
/// when \a fromEnd; an offset is a distance in points from that edge.
struct AxisPosition {
  bool fromEnd{false};
  bool percent{true};
  double value{50};
};
struct ContentPosition {
  AxisPosition x;
  AxisPosition y;
};

/// Reads an object ({ top, left, right, bottom }: numbers, "NN%", "center") or
/// a shorthand string ("top left", "bottom"). Missing axes center.
ContentPosition parseContentPosition(const QVariant& value);

/// Where the image is drawn inside a view of \a view points. \a imageScale is
/// pixels per point, so `none` draws the image at its natural point size.
QRectF fittedRect(QSizeF imagePixels, QSizeF view, ContentFit fit,
                  const ContentPosition& position, qreal imageScale);

/// Corner radii in the order top-left, top-right, bottom-right, bottom-left,
/// each limited so adjacent corners never overlap.
using CornerRadii = std::array<qreal, 4>;
QPainterPath roundedRectPath(const QRectF& rect, CornerRadii radii);

}  // namespace rnqnx::views
