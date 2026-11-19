#include "RnsvgPath.h"

#include <QPointF>
#include <algorithm>
#include <cmath>
#include <functional>

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
// `segment(end, startDirection, endDirection)` is called once per cubic the arc
// becomes. react-native-svg's own path parser also turns an arc into cubics
// and places markers at each cubic's end, so the vertices match it (the SVG
// text would put one at the arc's end only).
using SegmentSink = std::function<void(QPointF end, QPointF startDirection, QPointF endDirection)>;

bool arcTo(QPainterPath& path, QPointF p0, double rx, double ry, double xAxisRotation,
           bool largeArc, bool sweep, QPointF p1, const SegmentSink& segment) {
  if (p0 == p1) return false;
  rx = std::abs(rx);
  ry = std::abs(ry);
  if (rx == 0 || ry == 0) {
    path.lineTo(p1);
    segment(p1, p1 - p0, p1 - p0);
    return true;
  }
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
  QPointF segStart = p0;
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
    const QPointF end = i == segments - 1 ? p1 : map(e3);
    path.cubicTo(map(e1), map(e2), end);
    segment(end, map(e1) - segStart, end - map(e2));
    segStart = end;
    a += step;
  }
  return true;
}

QPointF unit(QPointF d) {
  const double len = std::hypot(d.x(), d.y());
  return len > 0 && std::isfinite(len) ? d / len : QPointF();
}

// The first of `candidates` that is not the zero vector, normalized.
QPointF firstDirection(std::initializer_list<QPointF> candidates) {
  for (const QPointF& c : candidates) {
    const QPointF u = unit(c);
    if (u.x() != 0 || u.y() != 0) return u;
  }
  return {};
}

// Records the vertices of a path as it is parsed: where each segment starts
// and ends and which way the path travels there.
class VertexRecorder {
 public:
  explicit VertexRecorder(std::vector<PathVertex>* out) : out_(out) {}

  void moveTo(QPointF p) {
    if (!out_) return;
    endSubpath();
    PathVertex v;
    v.point = p;
    v.subpathStart = true;
    out_->push_back(v);
    start_ = out_->size() - 1;
  }

  // A segment from the current vertex to `end`, leaving along `startDir` and
  // arriving along `endDir`.
  void segment(QPointF end, QPointF startDir, QPointF endDir) {
    if (!out_ || out_->empty()) return;
    out_->back().outDirection = unit(startDir);
    PathVertex v;
    v.point = end;
    v.inDirection = unit(endDir);
    out_->push_back(v);
  }

  // `z` from a point other than the subpath start adds the closing segment
  // and its vertex. When the path already ended at the start no vertex is
  // added; the closing segment has zero length, and resvg (the reference
  // renderer) gives such a segment the direction (1, 0). The first vertex of
  // a closed subpath keeps no incoming direction: neither renderer bisects a
  // closed subpath's start with its closing segment.
  void close(QPointF current, QPointF subpathStart) {
    if (!out_ || out_->empty()) return;
    if (current != subpathStart) {
      const QPointF d = subpathStart - current;
      segment(subpathStart, d, d);
    } else {
      out_->back().outDirection = QPointF(1, 0);
    }
    out_->back().closesSubpath = true;
    out_->back().subpathEnd = true;
    closed_ = true;
  }

  void finish() { endSubpath(); }

 private:
  void endSubpath() {
    if (!out_ || out_->empty()) return;
    if (!closed_) out_->back().subpathEnd = true;
    closed_ = false;
  }
  std::vector<PathVertex>* out_;
  size_t start_{0};
  bool closed_{false};
};

}  // namespace

QPainterPath parseSvgPath(const QString& data, bool* okOut, std::vector<PathVertex>* vertices) {
  if (vertices) vertices->clear();
  VertexRecorder rec(vertices);
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
      rec.close(cur, start);
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
        rec.moveTo(cur);
        start = cur;
        cmd = rel ? QChar('l') : QChar('L');  // further pairs are lineto
        break;
      }
      case 'L': {
        if (!read(2)) { reject(); break; }
        const QPointF to = rel ? cur + QPointF(n[0], n[1]) : QPointF(n[0], n[1]);
        path.lineTo(to);
        rec.segment(to, to - cur, to - cur);
        cur = to;
        break;
      }
      case 'H': {
        if (!read(1)) { reject(); break; }
        const QPointF to(rel ? cur.x() + n[0] : n[0], cur.y());
        path.lineTo(to);
        rec.segment(to, to - cur, to - cur);
        cur = to;
        break;
      }
      case 'V': {
        if (!read(1)) { reject(); break; }
        const QPointF to(cur.x(), rel ? cur.y() + n[0] : n[0]);
        path.lineTo(to);
        rec.segment(to, to - cur, to - cur);
        cur = to;
        break;
      }
      case 'C': {
        if (!read(6)) { reject(); break; }
        const QPointF o = rel ? cur : QPointF();
        const QPointF c1 = o + QPointF(n[0], n[1]), c2 = o + QPointF(n[2], n[3]), e = o + QPointF(n[4], n[5]);
        path.cubicTo(c1, c2, e);
        rec.segment(e, firstDirection({c1 - cur, c2 - cur, e - cur}), firstDirection({e - c2, e - c1, e - cur}));
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
        rec.segment(e, firstDirection({c1 - cur, c2 - cur, e - cur}), firstDirection({e - c2, e - c1, e - cur}));
        lastCtrl = c2;
        cur = e;
        break;
      }
      case 'Q': {
        if (!read(4)) { reject(); break; }
        const QPointF o = rel ? cur : QPointF();
        const QPointF c = o + QPointF(n[0], n[1]), e = o + QPointF(n[2], n[3]);
        path.quadTo(c, e);
        rec.segment(e, firstDirection({c - cur, e - cur}), firstDirection({e - c, e - cur}));
        lastCtrl = c;
        cur = e;
        break;
      }
      case 'T': {
        if (!read(2)) { reject(); break; }
        const QPointF c = (prev == 'Q' || prev == 'T') ? 2 * cur - lastCtrl : cur;
        const QPointF e = (rel ? cur : QPointF()) + QPointF(n[0], n[1]);
        path.quadTo(c, e);
        rec.segment(e, firstDirection({c - cur, e - cur}), firstDirection({e - c, e - cur}));
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
        arcTo(path, cur, rxv, ryv, rot, large, sweep, e,
              [&](QPointF end, QPointF startDirection, QPointF endDirection) {
                rec.segment(end, startDirection, endDirection);
              });
        cur = e;
        break;
      }
      default: reject(); break;
    }
    prev = up;
  }
  rec.finish();
  if (okOut) *okOut = ok;
  return path;
}

}  // namespace rnqnx::views::rnsvg
