#include "Decoration.h"
#include "PathGlyphs.h"

#include <map>
#include <memory>

namespace rnqnx::views::rnsvg::text::detail {

namespace {

double finiteOr(double v, double fallback) { return std::isfinite(v) ? v : fallback; }

// A decoration run: consecutive glyphs of one node on one baseline.
struct DecorationRun {
  bool open{false};
  double x0{0}, x1{0}, baseline{0};
};

class Emitter {
 public:
  Emitter(State& s, Result& out) : s_(s), out_(out), index_(s.nodes.size(), -1) {}

  void run() {
    for (size_t i = 0; i < s_.nodes.size(); ++i) {
      const FlatNode& f = s_.nodes[i];
      if (f.ownEndUnit == f.ownFirstUnit) continue;
      NodeOutput node;
      node.id = f.node->id;
      node.glyphs.setFillRule(Qt::WindingFill);
      index_[i] = int(out_.nodes.size());
      out_.nodes.push_back(std::move(node));
    }
    DecorationRun deco;
    int decoNode = -1;
    for (size_t i = 0; i < s_.units.size(); ++i) {
      const Unit& u = s_.units[i];
      if (decoNode != u.node) {
        flushDecoration(deco, decoNode);
        decoNode = u.node;
      }
      emit(u, deco);
    }
    flushDecoration(deco, decoNode);
  }

 private:
  const Probe& probeOf(int nodeIndex) {
    const Font& font = s_.nodes[size_t(nodeIndex)].node->font;
    return s_.probes.get(font, font.ligatures && finiteOr(font.letterSpacing, 0) == 0);
  }

  const PathRun& pathRun(int pathNode) {
    auto it = runs_.find(pathNode);
    if (it == runs_.end()) {
      it = runs_.emplace(pathNode, std::make_unique<PathRun>(*s_.nodes[size_t(pathNode)].node->textPath,
                                                           s_.truncated))
               .first;
    }
    return *it->second;
  }

  static void unite(QRectF& box, const QRectF& r) { box = box.isNull() ? r : box.united(r); }

  void flushDecoration(DecorationRun& run, int nodeIndex) {
    if (!run.open || nodeIndex < 0) return;
    run.open = false;
    const Font& font = s_.nodes[size_t(nodeIndex)].node->font;
    const unsigned flags = font.decoration;
    NodeOutput& out = out_.nodes[size_t(index_[size_t(nodeIndex)])];
    QPainterPath shape = decorationShape(flags, probeOf(nodeIndex).metrics, font.size, run.x1 - run.x0);
    out.decorations.addPath(shape.translated(run.x0, run.baseline));
  }

  void emit(const Unit& u, DecorationRun& deco) {
    if (index_[size_t(u.node)] < 0) return;
    NodeOutput& out = out_.nodes[size_t(index_[size_t(u.node)])];
    const Probe& probe = probeOf(u.node);
    const Font& font = s_.nodes[size_t(u.node)].node->font;
    const FontMetrics& m = probe.metrics;
    const double width = u.advance * u.scaleX;
    const QRectF em(0, -m.ascent * u.size, width, (m.ascent + m.descent) * u.size);

    if (u.pathNode < 0) {
      QTransform frame;
      frame.translate(u.pos, u.perp);
      frame.rotate(u.rotate);
      unite(out.advanceBox, frame.mapRect(em));
      if (!u.space && !u.outline.isEmpty()) {
        QTransform full = frame;
        full.scale(u.size * u.scaleX, u.size);
        out.glyphs.addPath(full.map(u.outline));
      }
      if (!u.space) out.placements.push_back({QPointF(u.pos, u.perp), u.rotate, width, u.charIndex});
      if (font.decoration != DecorationNone) extendRun(deco, u);
      return;
    }

    const Anchor anchor = u.chunk >= 0 ? s_.chunks[size_t(u.chunk)].anchor : s_.nodes[size_t(u.node)].node->anchor;
    const PathGlyph g = placeOnPath(pathRun(u.pathNode), u, anchor);
    if (!g.visible) return;
    unite(out.advanceBox, g.frame.mapRect(em));
    out.glyphs.addPath(g.outline);
    if (!u.space) out.placements.push_back({g.frame.map(QPointF(0, 0)), g.rotation, width, u.charIndex});
    if (font.decoration != DecorationNone) {
      // Each glyph carries its own piece, so the line follows the path.
      const double span = (u.advance + u.spacing) * u.scaleX + u.extra;
      out.decorations.addPath(g.frame.map(decorationShape(font.decoration, m, u.size, span)));
    }
  }

  void extendRun(DecorationRun& run, const Unit& u) {
    const double x0 = u.pos - u.kern * u.scaleX;
    const double tolerance = 1e-6 * std::max(1.0, u.size);
    if (run.open && std::abs(run.x1 - x0) <= tolerance && std::abs(run.baseline - u.perp) <= tolerance) {
      run.x1 = u.end;
      return;
    }
    flushDecoration(run, u.node);
    run.open = true;
    run.x0 = x0;
    run.x1 = u.end;
    run.baseline = u.perp;
  }

  State& s_;
  Result& out_;
  std::vector<int> index_;
  std::map<int, std::unique_ptr<PathRun>> runs_;
};

}  // namespace

void emitGeometry(State& s, Result& out) { Emitter(s, out).run(); }

}  // namespace rnqnx::views::rnsvg::text::detail
