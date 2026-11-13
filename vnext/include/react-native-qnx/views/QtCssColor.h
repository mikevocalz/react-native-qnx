#pragma once

#include <QColor>
#include <QRegularExpression>
#include <QString>

#include <algorithm>
#include <cmath>
#include <optional>

namespace rnqnx::views {

/// Parses a CSS color string: `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`,
/// `rgb()`, `rgba()` (comma or space separated, alpha as a number or percent),
/// or a color name Qt knows. CSS puts hex alpha last where QColor::setNamedColor
/// puts it first, so hex is parsed here. Returns nullopt for anything else.
inline std::optional<QColor> parseCssColor(const QString& input) {
  const QString text = input.trimmed();
  if (text.startsWith(QLatin1Char('#'))) {
    const QString hex = text.mid(1);
    bool ok = true;
    const auto pair = [&](int at, int width) {
      bool part = false;
      const uint value = hex.mid(at, width).toUInt(&part, 16);
      ok = ok && part;
      return width == 1 ? value * 17 : value;
    };
    if (hex.size() == 3 || hex.size() == 4) {
      const uint r = pair(0, 1), g = pair(1, 1), b = pair(2, 1);
      const uint a = hex.size() == 4 ? pair(3, 1) : 255;
      return ok ? std::optional<QColor>(QColor(int(r), int(g), int(b), int(a))) : std::nullopt;
    }
    if (hex.size() == 6 || hex.size() == 8) {
      const uint r = pair(0, 2), g = pair(2, 2), b = pair(4, 2);
      const uint a = hex.size() == 8 ? pair(6, 2) : 255;
      return ok ? std::optional<QColor>(QColor(int(r), int(g), int(b), int(a))) : std::nullopt;
    }
    return std::nullopt;
  }
  if (text.startsWith(QLatin1String("rgb"), Qt::CaseInsensitive)) {
    const int open = text.indexOf(QLatin1Char('('));
    const int close = text.lastIndexOf(QLatin1Char(')'));
    if (open < 0 || close < open) return std::nullopt;
    const auto parts = text.mid(open + 1, close - open - 1)
                           .split(QRegularExpression(QStringLiteral("[,/\\s]+")), Qt::SkipEmptyParts);
    if (parts.size() < 3) return std::nullopt;
    bool ok = true;
    const auto channel = [&](const QString& part) {
      bool number = false;
      const double value = part.endsWith(QLatin1Char('%')) ? part.chopped(1).toDouble(&number) * 2.55
                                                           : part.toDouble(&number);
      ok = ok && number;
      return value;
    };
    const double r = channel(parts[0]), g = channel(parts[1]), b = channel(parts[2]);
    double alpha = 1.0;
    if (parts.size() >= 4) {
      bool number = false;
      alpha = parts[3].endsWith(QLatin1Char('%')) ? parts[3].chopped(1).toDouble(&number) / 100.0
                                                  : parts[3].toDouble(&number);
      ok = ok && number;
    }
    if (!ok) return std::nullopt;
    const auto to8 = [](double v) { return std::clamp(int(std::lround(v)), 0, 255); };
    return QColor(to8(r), to8(g), to8(b), to8(alpha * 255.0));
  }
  const QColor named(text);
  if (named.isValid()) return named;
  return std::nullopt;
}

}  // namespace rnqnx::views
