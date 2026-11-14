#include "SvgValues.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg::testing {
namespace {

bool isSep(QChar c) { return c.isSpace() || c == QLatin1Char(','); }

void skipSeparators(const QString& s, qsizetype& pos) {
  while (pos < s.size() && isSep(s.at(pos))) ++pos;
}

}  // namespace

bool nextNumber(const QString& text, qsizetype& pos, double& out) {
  skipSeparators(text, pos);
  const qsizetype start = pos;
  qsizetype i = pos;
  if (i < text.size() && (text[i] == QLatin1Char('+') || text[i] == QLatin1Char('-'))) ++i;
  const qsizetype digitsStart = i;
  while (i < text.size() && text[i].isDigit()) ++i;
  if (i < text.size() && text[i] == QLatin1Char('.')) {
    ++i;
    while (i < text.size() && text[i].isDigit()) ++i;
  }
  if (i == digitsStart || (i == digitsStart + 1 && text[digitsStart] == QLatin1Char('.'))) return false;
  if (i < text.size() && (text[i] == QLatin1Char('e') || text[i] == QLatin1Char('E'))) {
    qsizetype j = i + 1;
    if (j < text.size() && (text[j] == QLatin1Char('+') || text[j] == QLatin1Char('-'))) ++j;
    if (j < text.size() && text[j].isDigit()) {
      while (j < text.size() && text[j].isDigit()) ++j;
      i = j;
    }
  }
  bool ok = false;
  const double v = text.mid(start, i - start).toDouble(&ok);
  if (!ok || !std::isfinite(v)) return false;
  out = v;
  pos = i;
  return true;
}

Length parseLength(const QString& raw) {
  const QString text = raw.trimmed();
  qsizetype pos = 0;
  double v = 0;
  if (!nextNumber(text, pos, v)) return {};
  const QString unit = text.mid(pos).trimmed();
  if (unit == QLatin1String("%")) return Length{v, Length::Unit::Percent, true};
  if (unit == QLatin1String("em")) return Length{v, Length::Unit::Em, true};
  double scale = 1;
  if (unit == QLatin1String("pt")) scale = 4.0 / 3.0;
  else if (unit == QLatin1String("pc")) scale = 16;
  else if (unit == QLatin1String("mm")) scale = 96.0 / 25.4;
  else if (unit == QLatin1String("cm")) scale = 96.0 / 2.54;
  else if (unit == QLatin1String("in")) scale = 96;
  return Length{v * scale, Length::Unit::Px, true};
}

std::vector<Length> parseLengthList(const QString& text) {
  std::vector<Length> out;
  const QString t = text.trimmed();
  if (t.isEmpty()) return out;
  const QStringList parts = QString(t).replace(QLatin1Char(','), QLatin1Char(' ')).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  for (const QString& p : parts) out.push_back(parseLength(p));
  return out;
}

std::vector<double> parseNumberList(const QString& text) {
  std::vector<double> out;
  qsizetype pos = 0;
  double v = 0;
  while (nextNumber(text, pos, v)) out.push_back(v);
  return out;
}

std::optional<QColor> parseColor(const QString& raw) {
  const QString text = raw.trimmed();
  if (text.isEmpty()) return std::nullopt;
  if (text.compare(QLatin1String("transparent"), Qt::CaseInsensitive) == 0) return QColor(0, 0, 0, 0);
  if (text.startsWith(QLatin1Char('#'))) {
    const QString hex = text.mid(1);
    bool ok = false;
    const uint value = hex.toUInt(&ok, 16);
    if (!ok) return std::nullopt;
    auto nib = [&](int i) { return int(hex.mid(i, 1).toUInt(nullptr, 16)) * 17; };
    auto byte = [&](int i) { return int(hex.mid(i, 2).toUInt(nullptr, 16)); };
    switch (hex.size()) {
      case 3: return QColor(nib(0), nib(1), nib(2));
      case 4: return QColor(nib(0), nib(1), nib(2), nib(3));
      case 6: return QColor(byte(0), byte(2), byte(4));
      case 8: return QColor(byte(0), byte(2), byte(4), byte(6));
      default: (void)value; return std::nullopt;
    }
  }
  static const QRegularExpression fn(QStringLiteral("^(rgba?|hsla?)\\(([^)]*)\\)$"), QRegularExpression::CaseInsensitiveOption);
  const auto m = fn.match(text);
  if (m.hasMatch()) {
    const QString kind = m.captured(1).toLower();
    const QStringList parts = m.captured(2).split(QRegularExpression(QStringLiteral("[\\s,/]+")), Qt::SkipEmptyParts);
    if (parts.size() < 3 || parts.size() > 4) return std::nullopt;
    auto channel = [](const QString& p, double full) {
      bool ok = false;
      if (p.endsWith(QLatin1Char('%'))) return p.chopped(1).toDouble(&ok) / 100.0 * full;
      return p.toDouble(&ok);
    };
    double alpha = 1;
    if (parts.size() == 4) {
      alpha = parts[3].endsWith(QLatin1Char('%')) ? parts[3].chopped(1).toDouble() / 100.0 : parts[3].toDouble();
    }
    alpha = std::clamp(alpha, 0.0, 1.0);
    if (kind.startsWith(QLatin1String("rgb"))) {
      auto c8 = [&](int i) { return std::clamp(int(std::lround(channel(parts[i], 255))), 0, 255); };
      return QColor(c8(0), c8(1), c8(2), int(std::lround(alpha * 255)));
    }
    const double h = parts[0].toDouble();
    const double s = channel(parts[1], 1.0) / (parts[1].endsWith(QLatin1Char('%')) ? 1.0 : 100.0);
    const double l = channel(parts[2], 1.0) / (parts[2].endsWith(QLatin1Char('%')) ? 1.0 : 100.0);
    QColor c = QColor::fromHslF(std::fmod(std::fmod(h, 360) + 360, 360) / 360.0, std::clamp(s, 0.0, 1.0), std::clamp(l, 0.0, 1.0));
    c.setAlphaF(alpha);
    return c;
  }
  const QColor named = QColor::fromString(text);
  if (named.isValid() && !text.startsWith(QLatin1Char('#'))) return named;
  return std::nullopt;
}

QString parseUrlId(const QString& raw) {
  const QString text = raw.trimmed();
  static const QRegularExpression urlRe(QStringLiteral("^url\\(\\s*#(.+?)\\s*\\)$"));
  if (const auto m = urlRe.match(text); m.hasMatch()) return m.captured(1);
  if (text.startsWith(QLatin1Char('#')) && text.size() > 1) return text.mid(1);
  return {};
}

Brush parseBrush(const QString& raw) {
  const QString text = raw.trimmed();
  Brush b;
  if (text == QLatin1String("none")) return b;
  if (text == QLatin1String("currentColor")) { b.type = Brush::Type::CurrentColor; return b; }
  if (text == QLatin1String("context-fill")) { b.type = Brush::Type::ContextFill; return b; }
  if (text == QLatin1String("context-stroke")) { b.type = Brush::Type::ContextStroke; return b; }
  static const QRegularExpression urlRe(QStringLiteral("^url\\(#(.+)\\)$"));
  if (const auto m = urlRe.match(text); m.hasMatch()) {
    b.type = Brush::Type::Ref;
    b.ref = m.captured(1);
    return b;
  }
  if (const auto c = parseColor(text)) {
    b.type = Brush::Type::Color;
    b.color = *c;
  }
  return b;
}

QTransform parseTransform(const QString& text, bool* okOut) {
  bool ok = true;
  QTransform total;
  qsizetype pos = 0;
  const qsizetype n = text.size();
  while (true) {
    skipSeparators(text, pos);
    if (pos >= n) break;
    qsizetype nameStart = pos;
    while (pos < n && text[pos].isLetter()) ++pos;
    const QString name = text.mid(nameStart, pos - nameStart);
    while (pos < n && text[pos].isSpace()) ++pos;
    if (name.isEmpty() || pos >= n || text[pos] != QLatin1Char('(')) { ok = false; break; }
    ++pos;
    std::vector<double> a;
    double v = 0;
    while (nextNumber(text, pos, v)) a.push_back(v);
    skipSeparators(text, pos);
    if (pos >= n || text[pos] != QLatin1Char(')')) { ok = false; break; }
    ++pos;
    QTransform t;
    const size_t c = a.size();
    if (name == QLatin1String("matrix") && c == 6) t = QTransform(a[0], a[1], a[2], a[3], a[4], a[5]);
    else if (name == QLatin1String("translate") && (c == 1 || c == 2)) t = QTransform::fromTranslate(a[0], c == 2 ? a[1] : 0);
    else if (name == QLatin1String("scale") && (c == 1 || c == 2)) t = QTransform::fromScale(a[0], c == 2 ? a[1] : a[0]);
    else if (name == QLatin1String("rotate") && (c == 1 || c == 3)) {
      t = c == 3 ? QTransform::fromTranslate(a[1], a[2]) : QTransform();
      t.rotate(a[0]);
      if (c == 3) t.translate(-a[1], -a[2]);
    } else if (name == QLatin1String("skewX") && c == 1) t = QTransform(1, 0, std::tan(a[0] * M_PI / 180.0), 1, 0, 0);
    else if (name == QLatin1String("skewY") && c == 1) t = QTransform(1, std::tan(a[0] * M_PI / 180.0), 0, 1, 0, 0);
    else { ok = false; break; }
    total = t * total;
  }
  if (okOut) *okOut = ok;
  return ok ? total : QTransform();
}

double parseOpacity(const QString& raw) {
  const QString text = raw.trimmed();
  double value = 0;
  bool ok = false;
  if (text.endsWith(QLatin1Char('%'))) value = text.chopped(1).toDouble(&ok) / 100.0;
  else value = text.toDouble(&ok);
  if (!ok || std::isnan(value) || value > 1) return 1;
  return std::max(value, 0.0);
}

ParsedViewBox parseViewBox(const QString& viewBox, const QString& preserveAspectRatio) {
  ParsedViewBox out;
  if (viewBox.trimmed().isEmpty()) return out;
  QString t = viewBox.trimmed();
  t.replace(QLatin1Char(','), QLatin1Char(' '));
  const QStringList p = t.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  if (p.size() != 4) return out;
  double v[4];
  for (int i = 0; i < 4; ++i) {
    bool ok = false;
    v[i] = p[i].toDouble(&ok);
    if (!ok) return out;
  }
  out.set = true;
  out.minX = v[0]; out.minY = v[1]; out.width = v[2]; out.height = v[3];
  const QStringList modes = preserveAspectRatio.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  static const QStringList aligns = {"xMinYMin", "xMidYMin", "xMaxYMin", "xMinYMid", "xMidYMid", "xMaxYMid", "xMinYMax", "xMidYMax", "xMaxYMax", "none"};
  if (!modes.isEmpty() && aligns.contains(modes[0])) out.align = modes[0];
  if (modes.size() > 1) {
    if (modes[1] == QLatin1String("slice")) out.meetOrSlice = 1;
    else if (modes[1] == QLatin1String("none")) out.meetOrSlice = 2;
  }
  return out;
}

}  // namespace rnqnx::views::rnsvg::testing
