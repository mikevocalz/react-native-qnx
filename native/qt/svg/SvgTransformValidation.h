#pragma once

#include <QString>
#include <QTransform>
#include <optional>

namespace rnqnx::views::detail {

std::optional<QTransform> parseSvgTransform(const QString& value);
bool isBoundedSvgTransform(const QTransform& transform);

}  // namespace rnqnx::views::detail
