#include "react-native-qnx/views/image/ImageLayout.h"

#include <QMap>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace rnqnx::views {
namespace {

// A value of an object-form position: a number (points) or "NN%" / "center".
AxisPosition axisFrom(const QVariant& raw, bool fromEnd) {
  AxisPosition axis;
  axis.fromEnd = fromEnd;
  if (raw.typeId() == QMetaType::QString) {
    const QString text = raw.toString().trimmed();
    if (text == QLatin1String("center")) {
      axis.percent = true;
      axis.value = 50;
      return axis;
    }
    bool ok = false;
    if (text.endsWith(QLatin1Char('%'))) {
      const double value = text.chopped(1).toDouble(&ok);
      if (ok) {
        axis.percent = true;
        axis.value = value;
        return axis;
      }
    }
    const double value = text.toDouble(&ok);
    if (ok) {
      axis.percent = false;
      axis.value = value;
      return axis;
    }
    return {};
  }
  bool ok = false;
  const double value = raw.toDouble(&ok);
  if (!ok) return {};
  axis.percent = false;
  axis.value = value;
  return axis;
}

ContentPosition fromWords(const QString& text) {
  ContentPosition position;
  const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString& word : words) {
    if (word == QLatin1String("left")) position.x = {false, false, 0};
    else if (word == QLatin1String("right")) position.x = {true, false, 0};
    else if (word == QLatin1String("top")) position.y = {false, false, 0};
    else if (word == QLatin1String("bottom")) position.y = {true, false, 0};
  }
  return position;
}

}  // namespace

ContentFit parseContentFit(const QString& text) {
  if (text == QLatin1String("contain")) return ContentFit::Contain;
  if (text == QLatin1String("fill")) return ContentFit::Fill;
  if (text == QLatin1String("none")) return ContentFit::None;
  if (text == QLatin1String("scale-down")) return ContentFit::ScaleDown;
  return ContentFit::Cover;
}

ContentPosition parseContentPosition(const QVariant& value) {
  if (value.typeId() == QMetaType::QString) return fromWords(value.toString());
  ContentPosition position;
  const QVariantMap map = value.toMap();
  if (map.contains(QStringLiteral("left"))) position.x = axisFrom(map.value(QStringLiteral("left")), false);
  else if (map.contains(QStringLiteral("right"))) position.x = axisFrom(map.value(QStringLiteral("right")), true);
  if (map.contains(QStringLiteral("top"))) position.y = axisFrom(map.value(QStringLiteral("top")), false);
  else if (map.contains(QStringLiteral("bottom"))) position.y = axisFrom(map.value(QStringLiteral("bottom")), true);
  return position;
}

QRectF fittedRect(QSizeF imagePixels, QSizeF view, ContentFit fit,
                  const ContentPosition& position, qreal imageScale) {
  if (imagePixels.isEmpty() || view.isEmpty()) return {};
  const qreal scale = imageScale > 0 ? imageScale : 1.0;
  const QSizeF natural(imagePixels.width() / scale, imagePixels.height() / scale);
  QSizeF drawn = natural;
  switch (fit) {
    case ContentFit::Fill:
      drawn = view;
      break;
    case ContentFit::Cover:
      drawn = natural.scaled(view, Qt::KeepAspectRatioByExpanding);
      break;
    case ContentFit::Contain:
      drawn = natural.scaled(view, Qt::KeepAspectRatio);
      break;
    case ContentFit::ScaleDown:
      if (natural.width() > view.width() || natural.height() > view.height()) {
        drawn = natural.scaled(view, Qt::KeepAspectRatio);
      }
      break;
    case ContentFit::None:
      break;
  }
  const auto place = [](const AxisPosition& axis, qreal free) {
    qreal offset = axis.percent ? free * axis.value / 100.0 : axis.value;
    return axis.fromEnd ? free - offset : offset;
  };
  return QRectF(place(position.x, view.width() - drawn.width()),
                place(position.y, view.height() - drawn.height()), drawn.width(), drawn.height());
}

QPainterPath roundedRectPath(const QRectF& rect, CornerRadii radii) {
  // CSS rule: when two radii on a side exceed it, scale all of them down.
  qreal factor = 1.0;
  const auto limit = [&factor](qreal sum, qreal length) {
    if (sum > length && sum > 0) factor = std::min(factor, length / sum);
  };
  limit(radii[0] + radii[1], rect.width());
  limit(radii[3] + radii[2], rect.width());
  limit(radii[0] + radii[3], rect.height());
  limit(radii[1] + radii[2], rect.height());
  for (qreal& radius : radii) radius = std::max<qreal>(0, radius * factor);

  QPainterPath path;
  const qreal l = rect.left(), t = rect.top(), r = rect.right(), b = rect.bottom();
  path.moveTo(l + radii[0], t);
  path.lineTo(r - radii[1], t);
  if (radii[1] > 0) path.arcTo(QRectF(r - 2 * radii[1], t, 2 * radii[1], 2 * radii[1]), 90, -90);
  path.lineTo(r, b - radii[2]);
  if (radii[2] > 0) path.arcTo(QRectF(r - 2 * radii[2], b - 2 * radii[2], 2 * radii[2], 2 * radii[2]), 0, -90);
  path.lineTo(l + radii[3], b);
  if (radii[3] > 0) path.arcTo(QRectF(l, b - 2 * radii[3], 2 * radii[3], 2 * radii[3]), 270, -90);
  path.lineTo(l, t + radii[0]);
  if (radii[0] > 0) path.arcTo(QRectF(l, t, 2 * radii[0], 2 * radii[0]), 180, -90);
  path.closeSubpath();
  return path;
}

}  // namespace rnqnx::views
