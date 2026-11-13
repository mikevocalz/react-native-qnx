#include "TextRenderer.h"

#include "Scene.h"

#include <QFont>
#include <QFontMetricsF>

#include <algorithm>
#include <cmath>

namespace rnqnx::views::rnsvg {
namespace {

constexpr double kTextProbeSize = 100;  // glyph outlines are built at this size, then scaled

QFont probeFont(const Style& st) {
  QFont font;
  if (!st.font.family.isEmpty()) font.setFamily(st.font.family);
  font.setPixelSize(int(kTextProbeSize));
  font.setWeight(QFont::Weight(std::clamp(st.font.weight.value_or(400), 1, 1000)));
  font.setItalic(st.font.italic.value_or(false));
  font.setHintingPreference(QFont::PreferNoHinting);
  const double size = st.font.size.value_or(12.0);
  if (st.font.letterSpacing && size > 0) {
    font.setLetterSpacing(QFont::AbsoluteSpacing, *st.font.letterSpacing * kTextProbeSize / size);
  }
  return font;
}

struct Cursor {
  QPointF pos;
};

// Walks the subtree once, calling `emit(element, frame, outline)` for each node
// that has glyphs.
template <class Emit>
void walk(Scene& scene, const RnsvgElementItem* e, const Frame& f, int depth, Cursor& cur, bool top, Emit& emit) {
  if (depth > kMaxDepth) return;
  const auto& p = e->props();
  if (p.displayNone) return;
  const Units units(f);
  if (top) cur.pos = QPointF(0, 0);
  if (!p.text.x.empty()) cur.pos.setX(units.resolve(p.text.x.front(), Axis::X));
  if (!p.text.y.empty()) cur.pos.setY(units.resolve(p.text.y.front(), Axis::Y));
  if (!p.text.dx.empty()) cur.pos.rx() += units.resolve(p.text.dx.front(), Axis::X);
  if (!p.text.dy.empty()) cur.pos.ry() += units.resolve(p.text.dy.front(), Axis::Y);
  if (!p.content.isEmpty()) {
    const QFont font = probeFont(f.style);
    const double size = f.style.font.size.value_or(12.0);
    const double k = size / kTextProbeSize;
    const QFontMetricsF fm(font);
    const double advance = fm.horizontalAdvance(p.content) * k;
    double x = cur.pos.x();
    if (f.style.font.anchor == QStringLiteral("middle")) x -= advance / 2;
    else if (f.style.font.anchor == QStringLiteral("end")) x -= advance;
    QPainterPath glyphs;
    glyphs.addText(QPointF(0, 0), font, p.content);
    const QPainterPath path = QTransform::fromTranslate(x, cur.pos.y()).scale(k, k).map(glyphs);
    cur.pos.rx() += advance;
    if (!path.isEmpty()) emit(e, f, path);
  }
  for (auto* c : e->elementChildren()) {
    const auto& cp = c->props();
    if (cp.kind != Kind::Text && cp.kind != Kind::TSpan && cp.kind != Kind::TextPath) continue;
    Frame cf = f;
    applyProps(cf.style, cp);
    if (cp.matrix) cf.ctm = *cp.matrix * cf.ctm;
    if (cp.responsible) scene.pushResponsible(c);
    walk(scene, c, cf, depth + 1, cur, false, emit);
    if (cp.responsible) scene.popResponsible();
  }
}

}  // namespace

QPainterPath TextRenderer::outline(const RnsvgElementItem* e, const Frame& f, int depth) {
  QPainterPath all;
  Cursor cur;
  auto emit = [&](const RnsvgElementItem*, const Frame&, const QPainterPath& path) { all.addPath(path); };
  walk(scene_, e, f, depth, cur, true, emit);
  return all;
}

void TextRenderer::draw(const RnsvgElementItem& e, const Frame& f, int depth) {
  Cursor cur;
  auto emit = [&](const RnsvgElementItem* node, const Frame& nf, const QPainterPath& path) {
    const auto& np = node->props();
    if (scene_.hitMode()) {
      scene_.hitTester().testPath(node, path, nf, PointerMode::parse(np.pointerEvents));
      return;
    }
    const QRectF bbox = path.boundingRect();
    EffectBoxes boxes;
    boxes.objectBox = [bbox] { return bbox; };
    boxes.paintBox = [bbox] { return bbox; };
    const bool both = nf.style.fill.type != Brush::Type::None && nf.style.stroke.type != Brush::Type::None;
    scene_.effects().paint(*node, nf, boxes, both, depth, [&] { scene_.shapes().draw(path, nf, bbox); });
  };
  walk(scene_, &e, f, depth, cur, true, emit);
}

}  // namespace rnqnx::views::rnsvg
