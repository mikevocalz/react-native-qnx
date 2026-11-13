#include "RnsvgPath.h"

#include <QPointF>
#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

constexpr double kPi = 3.14159265358979323846;

class Scanner {
 public:
  explicit Scanner(const QString& s) : s_(s) {}
  bool atEnd() { skipSeparators(); return i_ >= s_.size(); }
  bool peekCommand(QChar& c) {
    skipSeparators();
    if (i_ >= s_.size()) return false;
    const QChar ch = s_[i_];
    if (QStringLiteral("MmLlHhVvCcSsQqTtAaZz").contains(ch)) { c = ch; return true; }
    return false;
  }
  void consumeCommand() { ++i_; }
  bool number(double& out) {
    skipSeparators();
    const int start = i_;
    int j = i_;
    if (j < s_.size() && (s_[j] == '+' || s_[j] == '-')) ++j;
    bool digits = false;
    while (j < s_.size() && s_[j].isDigit()) { ++j; digits = true; }
    if (j < s_.size() && s_[j] == '.') {
      ++j;
      while (j < s_.size() && s_[j].isDigit()) { ++j; digits = true; }
    }
    if (!digits) return false;
    if (j < s_.size() && (s_[j] == 'e' || s_[j] == 'E')) {
      int k = j + 1;
      if (k < s_.size() && (s_[k] == '+' || s_[k] == '-')) ++k;
      if (k < s_.size() && s_[k].isDigit()) {
        while (k < s_.size() && s_[k].isDigit()) ++k;
        j = k;
      }
    }
    bool ok = false;
    out = s_.mid(start, j - start).toDouble(&ok);
    i_ = j;
    return ok && std::isfinite(out);
  }
  // Arc flags are single characters and may be packed ("a1 1 0 11 5 5").
  bool flag(bool& out) {
    skipSeparators();
    if (i_ < s_.size() && (s_[i_] == '0' || s_[i_] == '1')) { out = s_[i_] == '1'; ++i_; return true; }
    return false;
  }

 private:
  void skipSeparators() {
    while (i_ < s_.size() && (s_[i_].isSpace() || s_[i_] == ',')) ++i_;
  }
  const QString& s_;
  int i_{0};
};

// SVG implementation notes F.6: endpoint to center parameterization.
void arcTo(QPainterPath& path, QPointF p0, double rx, double ry, double xAxisRotation,
           bool largeArc, bool sweep, QPointF p1) {
  if (p0 == p1) return;
  rx = std::abs(rx);
  ry = std::abs(ry);
  if (rx == 0 || ry == 0) { path.lineTo(p1); return; }
  const double phi = xAxisRotation * kPi / 180.0;
  const double cosPhi = std::cos(phi), sinPhi = std::sin(phi);
  const double dx2 = (p0.x() - p1.x()) / 2.0, dy2 = (p0.y() - p1.y()) / 2.0;
  const double x1p = cosPhi * dx2 + sinPhi * dy2;
  const double y1p = -sinPhi * dx2 + cosPhi * dy2;
  const double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
  if (lambda > 1) { const double s = std::sqrt(lambda); rx *= s; ry *= s; }
  const double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
  const double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
  double coef = den == 0 ? 0 : std::sqrt(std::max(0.0, num / den));
  if (largeArc == sweep) coef = -coef;
  const double cxp = coef * (rx * y1p / ry);
  const double cyp = coef * -(ry * x1p / rx);
  const double cx = cosPhi * cxp - sinPhi * cyp + (p0.x() + p1.x()) / 2.0;
  const double cy = sinPhi * cxp + cosPhi * cyp + (p0.y() + p1.y()) / 2.0;
  auto angle = [](double ux, double uy, double vx, double vy) {
    const double dot = ux * vx + uy * vy;
    const double len = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
    double a = std::acos(std::clamp(len == 0 ? 1.0 : dot / len, -1.0, 1.0));
    if (ux * vy - uy * vx < 0) a = -a;
    return a;
  };
  const double theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
  double delta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
  if (!sweep && delta > 0) delta -= 2 * kPi;
  if (sweep && delta < 0) delta += 2 * kPi;
  // Split into <= 90 degree segments and approximate each with a cubic.
  const int segments = std::max(1, int(std::ceil(std::abs(delta) / (kPi / 2) - 1e-9)));
  const double step = delta / segments;
  const double t = 4.0 / 3.0 * std::tan(step / 4.0);
  double a = theta1;
  for (int i = 0; i < segments; ++i) {
    const double cosA = std::cos(a), sinA = std::sin(a);
    const double cosB = std::cos(a + step), sinB = std::sin(a + step);
    const QPointF e1(cosA - t * sinA, sinA + t * cosA);
    const QPointF e2(cosB + t * sinB, sinB - t * cosB);
    const QPointF e3(cosB, sinB);
    auto map = [&](QPointF q) {
      const double x = rx * q.x(), y = ry * q.y();
      return QPointF(cosPhi * x - sinPhi * y + cx, sinPhi * x + cosPhi * y + cy);
    };
    path.cubicTo(map(e1), map(e2), i == segments - 1 ? p1 : map(e3));
    a += step;
  }
}

}  // namespace

QPainterPath parseSvgPath(const QString& data, bool* okOut) {
  QPainterPath path;
  bool ok = true;
  Scanner in(data);
  QPointF cur, start, lastCtrl;
  QChar cmd, prev;
  int tokens = 0;
  auto reject = [&] { ok = false; };
  while (ok && !in.atEnd()) {
    if (++tokens > kMaxPathTokens) { reject(); break; }
    QChar next;
    if (in.peekCommand(next)) {
      cmd = next;
      in.consumeCommand();
    } else if (cmd.isNull() || cmd == 'Z' || cmd == 'z') {
      reject();
      break;
    }  // otherwise an implicit repeat of the previous command
    const bool rel = cmd.isLower();
    const QChar up = cmd.toUpper();
    double n[7];
    auto read = [&](int count) {
      for (int i = 0; i < count; ++i) if (!in.number(n[i])) return false;
      return true;
    };
    if (up == 'Z') {
      path.closeSubpath();
      cur = start;
      prev = up;
      continue;
    }
    if (path.elementCount() == 0 && up != 'M') { reject(); break; }
    switch (up.unicode()) {
      case 'M': {
        if (!read(2)) { reject(); break; }
        cur = rel ? cur + QPointF(n[0], n[1]) : QPointF(n[0], n[1]);
        path.moveTo(cur);
        start = cur;
        cmd = rel ? QChar('l') : QChar('L');  // further pairs are lineto
        break;
      }
      case 'L': {
        if (!read(2)) { reject(); break; }
        cur = rel ? cur + QPointF(n[0], n[1]) : QPointF(n[0], n[1]);
        path.lineTo(cur);
        break;
      }
      case 'H': {
        if (!read(1)) { reject(); break; }
        cur.setX(rel ? cur.x() + n[0] : n[0]);
        path.lineTo(cur);
        break;
      }
      case 'V': {
        if (!read(1)) { reject(); break; }
        cur.setY(rel ? cur.y() + n[0] : n[0]);
        path.lineTo(cur);
        break;
      }
      case 'C': {
        if (!read(6)) { reject(); break; }
        const QPointF o = rel ? cur : QPointF();
        const QPointF c1 = o + QPointF(n[0], n[1]), c2 = o + QPointF(n[2], n[3]), e = o + QPointF(n[4], n[5]);
        path.cubicTo(c1, c2, e);
        lastCtrl = c2;
        cur = e;
        break;
      }
      case 'S': {
        if (!read(4)) { reject(); break; }
        const QPointF o = rel ? cur : QPointF();
        const QPointF c1 = (prev == 'C' || prev == 'S') ? 2 * cur - lastCtrl : cur;
        const QPointF c2 = o + QPointF(n[0], n[1]), e = o + QPointF(n[2], n[3]);
        path.cubicTo(c1, c2, e);
        lastCtrl = c2;
        cur = e;
        break;
      }
      case 'Q': {
        if (!read(4)) { reject(); break; }
        const QPointF o = rel ? cur : QPointF();
        const QPointF c = o + QPointF(n[0], n[1]), e = o + QPointF(n[2], n[3]);
        path.quadTo(c, e);
        lastCtrl = c;
        cur = e;
        break;
      }
      case 'T': {
        if (!read(2)) { reject(); break; }
        const QPointF c = (prev == 'Q' || prev == 'T') ? 2 * cur - lastCtrl : cur;
        const QPointF e = (rel ? cur : QPointF()) + QPointF(n[0], n[1]);
        path.quadTo(c, e);
        lastCtrl = c;
        cur = e;
        break;
      }
      case 'A': {
        double rxv = 0, ryv = 0, rot = 0, ex = 0, ey = 0;
        bool large = false, sweep = false;
        if (!in.number(rxv) || !in.number(ryv) || !in.number(rot) || !in.flag(large) ||
            !in.flag(sweep) || !in.number(ex) || !in.number(ey)) {
          reject();
          break;
        }
        const QPointF e = (rel ? cur : QPointF()) + QPointF(ex, ey);
        arcTo(path, cur, rxv, ryv, rot, large, sweep, e);
        cur = e;
        break;
      }
      default: reject(); break;
    }
    prev = up;
  }
  if (okOut) *okOut = ok;
  return path;
}

}  // namespace rnqnx::views::rnsvg
