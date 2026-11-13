#pragma once

#include "../RnsvgItems.h"
#include "Bounds.h"
#include "ClipMask.h"
#include "Effects.h"
#include "FilterRunner.h"
#include "HitTester.h"
#include "ImageRenderer.h"
#include "Markers.h"
#include "PaintServers.h"
#include "ShapePainter.h"
#include "Style.h"
#include "TextRenderer.h"

#include <QPainter>
#include <optional>
#include <unordered_map>

#include <vector>

namespace rnqnx::views::rnsvg {

// Where the scene draws: a painter, the transform from item space to that
// painter's device pixels, and the device-space rectangle worth drawing in.
// Layers (group opacity, masks, filters, patterns) push a smaller target.
struct Target {
  QPainter* painter{nullptr};
  QTransform base;
  QRectF clip;
};

// Caps for hostile documents: elements visited per paint or hit test, and
// nesting of groups, <use> chains and mask recursion.
inline constexpr int kMaxVisits = 200000;
inline constexpr int kMaxDepth = 32;

// One paint or hit test over a root's retained element tree. It owns the
// traversal and the state that is shared across elements; each collaborator
// (paint servers, clip and mask, markers, text, effects, hit testing) owns one
// kind of work and reaches back here through the small public surface below.
class Scene {
 public:
  Scene(const RnsvgRootItem& root, QPainter* painter, std::optional<QPointF> hit);

  void run();
  const RnsvgElementItem* hitTarget() const { return hitTarget_; }

  // ---- surface for the collaborators --------------------------------------
  const RnsvgElementItem* byId(const QString& id) const;
  Target& target() { return target_; }
  QPainter* painter() const { return target_.painter; }
  bool hitMode() const { return hit_.has_value(); }
  const std::optional<QPointF>& hitPoint() const { return hit_; }
  const RootProps& rootProps() const { return root_.rootProps(); }
  QSizeF itemSize() const { return QSizeF(root_.width(), root_.height()); }

  void render(const RnsvgElementItem* e, const Frame& parent, int depth);
  void renderChildren(const RnsvgElementItem* e, const Frame& f, int depth);
  // Runs `fn` with `t` as the target, restoring the previous one after.
  template <class Fn>
  void withTarget(const Target& t, Fn&& fn) {
    const Target saved = target_;
    target_ = t;
    fn();
    target_ = saved;
  }
  // Same, with hit testing switched off (mask and pattern content paints).
  template <class Fn>
  void withoutHit(Fn&& fn) {
    const auto saved = hit_;
    hit_.reset();
    fn();
    hit_ = saved;
  }
  // Records `e`, or its nearest responsible ancestor, as the hit target.
  void noteHit(const RnsvgElementItem* e) {
    const RnsvgElementItem* target = e->props().responsible ? e : nullptr;
    for (auto it = responsible_.rbegin(); !target && it != responsible_.rend(); ++it) target = *it;
    if (target) hitTarget_ = target;
  }
  void pushResponsible(const RnsvgElementItem* e) { responsible_.push_back(e); }
  void popResponsible() { responsible_.pop_back(); }

  // context-fill / context-stroke: the paint of the element a marker or a
  // <use> is instantiated for.
  struct ContextPaint {
    Brush fill, stroke;
    QRectF bbox;
    Frame frame;
  };
  void pushContext(const ContextPaint& c) { context_.push_back(c); }
  void popContext() { context_.pop_back(); }
  const ContextPaint* context() const { return context_.empty() ? nullptr : &context_.back(); }

  Bounds& bounds() { return bounds_; }
  PaintServers& paintServers() { return paintServers_; }
  ShapePainter& shapes() { return shapes_; }
  ClipMask& clipMask() { return clipMask_; }
  Markers& markers() { return markers_; }
  HitTester& hitTester() { return hitTester_; }
  Effects& effects() { return effects_; }
  TextRenderer& text() { return text_; }
  ImageRenderer& images() { return images_; }
  FilterRunner& filters() { return filters_; }

  static void configure(QPainter& p) {
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
  }

 private:
  void collect(const RnsvgElementItem* e, int depth);
  void renderShape(const RnsvgElementItem* e, const Frame& f, const Frame& parent, int depth);
  void renderGroup(const RnsvgElementItem* e, const Frame& f, const Frame& parent, int depth);
  void renderUse(const RnsvgElementItem* e, const Frame& f, const Frame& parent, int depth);
  // A <use> of a <symbol>: the symbol's viewBox laid into the use's width and
  // height (default 100%), content clipped to that viewport.
  Frame symbolFrame(const RnsvgElementItem& symbol, const Frame& useFrame, const Units& units,
                    const ElementProps& use) const;
  void renderSymbol(const RnsvgElementItem& symbol, const Frame& useFrame, const Units& units,
                    const ElementProps& use, int depth);

  const RnsvgRootItem& root_;
  std::optional<QPointF> hit_;
  Target target_;
  QTransform view_;
  QSizeF viewport_;
  QColor rootColor_{Qt::black};
  struct QStringHash {
    size_t operator()(const QString& s) const { return qHash(s); }
  };
  std::unordered_map<QString, const RnsvgElementItem*, QStringHash> ids_;
  std::vector<const RnsvgElementItem*> responsible_;
  std::vector<const RnsvgElementItem*> active_;
  std::vector<ContextPaint> context_;
  int visits_{0};
  const RnsvgElementItem* hitTarget_{nullptr};

  Bounds bounds_;
  PaintServers paintServers_;
  ShapePainter shapes_;
  ClipMask clipMask_;
  Markers markers_;
  HitTester hitTester_;
  Effects effects_;
  TextRenderer text_;
  ImageRenderer images_;
  FilterRunner filters_;
};

}  // namespace rnqnx::views::rnsvg
