#include "TextRenderer.h"

#include "../text/TextLayout.h"
#include "../text/TextParse.h"
#include "Geometry.h"
#include "Scene.h"

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

// What the scene needs back from a laid-out node: the element it came from and
// the frame its paint is resolved in.
struct NodeEntry {
  const RnsvgElementItem* element;
  Frame frame;
};

class TreeBuilder {
 public:
  TreeBuilder(Scene& scene, std::vector<NodeEntry>& entries) : scene_(scene), entries_(entries) {}

  text::Node build(const RnsvgElementItem* e, const Frame& f, int depth) {
    text::Node n;
    n.id = int(entries_.size());
    entries_.push_back({e, f});
    const ElementProps& p = e->props();
    const Units units(f);
    fillFont(n, f);
    n.content = p.content;
    for (const Length& l : p.text.x) n.x.push_back(optionalLength(l, units, Axis::X));
    for (const Length& l : p.text.y) n.y.push_back(optionalLength(l, units, Axis::Y));
    for (const Length& l : p.text.dx) n.dx.push_back(optionalLength(l, units, Axis::X));
    for (const Length& l : p.text.dy) n.dy.push_back(optionalLength(l, units, Axis::Y));
    n.rotate = p.text.rotate;
    if (p.text.textLength.set) n.textLength = units.resolve(p.text.textLength, Axis::X);
    n.lengthAdjust = text::parseLengthAdjust(p.text.lengthAdjust);
    if (!p.text.baselineShiftKeyword.isEmpty()) {
      n.baselineShift = text::parseBaselineShiftKeyword(p.text.baselineShiftKeyword);
    } else if (p.text.baselineShift.set) {
      // A percentage is of the font size, as upstream reads it.
      const Length& l = p.text.baselineShift;
      n.baselineShift = l.unit == Length::Unit::Percent ? l.value / 100.0 * n.font.size : units.resolve(l, Axis::Y);
    }
    n.alignmentBaseline = text::parseAlignmentBaseline(p.text.alignmentBaseline);
    if (p.text.inlineSize.set) n.inlineSize = units.resolve(p.text.inlineSize, Axis::X);
    if (p.kind == Kind::TextPath) n.textPath = pathSpec(p, units, f);
    if (depth < kMaxDepth) {
      for (const RnsvgElementItem* c : e->elementChildren()) {
        const Kind k = c->props().kind;
        if (k != Kind::Text && k != Kind::TSpan && k != Kind::TextPath) continue;
        if (c->props().displayNone) continue;
        Frame cf = f;
        applyProps(cf.style, c->props());
        n.children.push_back(build(c, cf, depth + 1));
      }
    }
    return n;
  }

 private:
  static std::optional<double> optionalLength(const Length& l, const Units& u, Axis axis) {
    if (!l.set) return std::nullopt;
    return u.resolve(l, axis);
  }

  static void fillFont(text::Node& n, const Frame& f) {
    const FontSpec& fs = f.style.font;
    text::Font& font = n.font;
    font.family = fs.family;
    font.size = fs.size.value_or(12.0);
    font.weight = std::clamp(fs.weight.value_or(400), 100, 900);
    font.italic = fs.italic.value_or(false);
    font.stretchPercent = fs.fontStretch.isEmpty() ? 100.0 : text::parseStretch(fs.fontStretch);
    font.letterSpacing = fs.letterSpacing.value_or(0.0);
    font.wordSpacing = fs.wordSpacing.value_or(0.0);
    font.kerning = fs.kerning;
    font.ligatures = fs.variantLigatures != QStringLiteral("none");
    font.features = text::parseFeatureSettings(fs.fontFeatureSettings);
    font.decoration = text::parseDecoration(fs.decoration);
    n.anchor = text::parseAnchor(fs.anchor);
  }

  std::optional<text::TextPathSpec> pathSpec(const ElementProps& p, const Units& units, const Frame& f) {
    const RnsvgElementItem* ref = scene_.byId(p.textPath.href);
    if (!ref || !isShape(ref->props().kind)) return std::nullopt;
    QPainterPath path = shapePath(ref->props(), units);
    if (ref->props().matrix) path = ref->props().matrix->map(path);
    text::TextPathSpec spec;
    spec.path = path;
    const Length& so = p.textPath.startOffset;
    if (so.set) {
      spec.startOffset.percent = so.unit == Length::Unit::Percent;
      spec.startOffset.value = spec.startOffset.percent ? so.value : units.resolve(so, Axis::X);
    }
    spec.side = text::parseSide(p.textPath.side);
    spec.method = text::parseMethod(p.textPath.method);
    spec.spacing = text::parseSpacing(p.textPath.spacing);
    spec.midLine = text::parseMidLine(p.textPath.midLine);
    Q_UNUSED(f);
    return spec;
  }

  Scene& scene_;
  std::vector<NodeEntry>& entries_;
};

}  // namespace

QPainterPath TextRenderer::outline(const RnsvgElementItem* e, const Frame& f, int depth) {
  if (depth > kMaxDepth) return {};
  std::vector<NodeEntry> entries;
  const text::Node root = TreeBuilder(scene_, entries).build(e, f, 0);
  const text::Result result = text::layout(root, QPointF(0, 0));
  QPainterPath all;
  for (const auto& node : result.nodes) {
    all.addPath(node.glyphs);
    all.addPath(node.decorations);
  }
  return all;
}

QRectF TextRenderer::cellBox(const RnsvgElementItem* e, const Frame& f, int depth) {
  if (depth > kMaxDepth) return {};
  std::vector<NodeEntry> entries;
  const text::Node root = TreeBuilder(scene_, entries).build(e, f, 0);
  const text::Result result = text::layout(root, QPointF(0, 0));
  QRectF box;
  for (const auto& node : result.nodes) {
    if (node.advanceBox.isNull()) continue;
    box = box.isNull() ? node.advanceBox : box.united(node.advanceBox);
  }
  return box;
}

void TextRenderer::draw(const RnsvgElementItem& e, const Frame& f, int depth) {
  std::vector<NodeEntry> entries;
  const text::Node root = TreeBuilder(scene_, entries).build(&e, f, 0);
  const text::Result result = text::layout(root, QPointF(0, 0));

  auto drawNodes = [&] {
    for (const auto& out : result.nodes) {
      if (out.id < 0 || size_t(out.id) >= entries.size()) continue;
      const NodeEntry& entry = entries[size_t(out.id)];
      QPainterPath path = out.glyphs;
      path.addPath(out.decorations);
      if (path.isEmpty()) continue;
      const RnsvgElementItem* node = entry.element;
      const ElementProps& np = node->props();
      const bool nested = node != &e;
      // A nested node's own matrix, press target and effects apply to it; the
      // <Text> element's already went through the scene.
      Frame nf = entry.frame;
      if (nested && np.matrix) nf.ctm = *np.matrix * nf.ctm;
      const bool pushed = nested && np.responsible;
      if (pushed) scene_.pushResponsible(node);
      const QRectF bbox = out.advanceBox.isNull() ? path.boundingRect() : out.advanceBox;
      if (scene_.hitMode()) {
        scene_.hitTester().testPath(node, path, nf, PointerMode::parse(np.pointerEvents));
      } else if (!nested) {
        scene_.shapes().draw(path, nf, bbox);
      } else {
        EffectBoxes boxes;
        boxes.objectBox = [bbox] { return bbox; };
        boxes.paintBox = [path] { return path.boundingRect().adjusted(-2, -2, 2, 2); };
        const bool both = nf.style.fill.type != Brush::Type::None && nf.style.stroke.type != Brush::Type::None;
        scene_.effects().paint(*node, nf, boxes, both, depth, [&] { scene_.shapes().draw(path, nf, bbox); });
      }
      if (pushed) scene_.popResponsible();
    }
  };

  if (scene_.hitMode()) {
    drawNodes();
    return;
  }
  QPainterPath all;
  QRectF cells;
  for (const auto& out : result.nodes) {
    all.addPath(out.glyphs);
    all.addPath(out.decorations);
    if (!out.advanceBox.isNull()) cells = cells.isNull() ? out.advanceBox : cells.united(out.advanceBox);
  }
  if (all.isEmpty()) return;
  const QRectF ink = all.boundingRect();
  const QRectF bbox = cells.isNull() ? ink : cells;
  EffectBoxes boxes;
  boxes.objectBox = [bbox] { return bbox; };
  boxes.paintBox = [ink] { return ink.adjusted(-2, -2, 2, 2); };
  scene_.effects().paint(e, f, boxes, true, depth, drawNodes);
}

}  // namespace rnqnx::views::rnsvg
