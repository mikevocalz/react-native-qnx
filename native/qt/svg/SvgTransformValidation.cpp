#include "SvgTransformValidation.h"
#include "SvgDocument.h"

#include <QRegularExpression>
#include <QtMath>
#include <cmath>

namespace rnqnx::views::detail {

bool isBoundedSvgTransform(const QTransform& transform) {
  for (const double value : {transform.m11(), transform.m12(), transform.m21(),
                            transform.m22(), transform.dx(), transform.dy()}) {
    if (!std::isfinite(value) || std::abs(value) > kSvgMaxCoordinate) return false;
  }
  return true;
}

std::optional<QTransform> parseSvgTransform(const QString& value) {
  static const QRegularExpression operation("\\G\\s*,?\\s*([A-Za-z]+)\\s*\\(([^)]*)\\)\\s*");
  qsizetype position = 0;
  int count = 0;
  QTransform transform;
  while (position < value.size()) {
    const auto match = operation.match(value, position);
    if (!match.hasMatch() || ++count > 32) return std::nullopt;
    const auto words = match.captured(2).split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
    QVector<double> args;
    for (const auto& word : words) {
      bool ok = false;
      const double number = word.toDouble(&ok);
      if (!ok || !std::isfinite(number) || std::abs(number) > kSvgMaxCoordinate) return std::nullopt;
      args.append(number);
    }
    const auto name = match.captured(1);
    const auto size = args.size();
    QTransform next;
    if (name == "matrix" && size == 6) {
      next = QTransform(args[0], args[1], args[2], args[3], args[4], args[5]);
    } else if (name == "translate" && (size == 1 || size == 2)) {
      next.translate(args[0], size == 2 ? args[1] : 0);
    } else if (name == "scale" && (size == 1 || size == 2)) {
      next.scale(args[0], size == 2 ? args[1] : args[0]);
    } else if (name == "rotate" && (size == 1 || size == 3)) {
      if (size == 3) next.translate(args[1], args[2]);
      next.rotate(args[0]);
      if (size == 3) next.translate(-args[1], -args[2]);
    } else if ((name == "skewX" || name == "skewY") && size == 1) {
      const double skew = std::tan(qDegreesToRadians(args[0]));
      next.shear(name == "skewX" ? skew : 0, name == "skewY" ? skew : 0);
    } else {
      return std::nullopt;
    }
    // Validate intermediate products as well as the final transform. A later
    // inverse must not hide an excessive intermediate scale or translation.
    transform = next * transform;
    if (!isBoundedSvgTransform(next) || !isBoundedSvgTransform(transform)) return std::nullopt;
    position = match.capturedEnd();
  }
  return count > 0 ? std::optional<QTransform>(transform) : std::nullopt;
}

}  // namespace rnqnx::views::detail
