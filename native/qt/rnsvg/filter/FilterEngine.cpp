#include "FilterEngine.h"

#include "FilterColorSpace.h"
#include "FilterInternal.h"
#include "FilterPrimitives.h"
#include "FilterRegion.h"

#include <QPainter>
#include <optional>
#include <type_traits>
#include <variant>

namespace rnqnx::views::rnsvg::filter {
namespace detail {
namespace {

// Where a primitive input comes from.
struct InputRef {
  enum class Kind { SourceGraphic, SourceAlpha, Empty, FillPaint, StrokePaint, Result };
  Kind kind{Kind::SourceGraphic};
  int index{-1};  // Result: the producing primitive
};

struct PrimitiveInputs {
  InputRef in1, in2;
  std::vector<InputRef> nodes;
};

bool usesIn1(const Params& p) {
  return !(std::holds_alternative<Flood>(p) || std::holds_alternative<Turbulence>(p) ||
           std::holds_alternative<Image>(p) || std::holds_alternative<Merge>(p));
}

bool usesIn2(const Params& p) {
  return std::holds_alternative<Blend>(p) || std::holds_alternative<Composite>(p) ||
         std::holds_alternative<DisplacementMap>(p);
}

QImage normalizedSource(const QImage& source, const QSize& size) {
  if (source.format() == QImage::Format_ARGB32_Premultiplied && source.size() == size) return source;
  QImage out = makeImage(size);
  if (source.isNull()) return out;
  QPainter painter(&out);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.drawImage(QPoint(0, 0), source.convertToFormat(QImage::Format_ARGB32_Premultiplied));
  return out;
}

class Evaluator {
 public:
  Evaluator(const Env& env, std::vector<const Primitive*> primitives, const QImage& source)
      : env_(env), prims_(std::move(primitives)), source_(normalizedSource(source, env.region.size())),
        full_(QPoint(0, 0), env.region.size()) {}

  Result run() {
    if (prims_.empty()) return finish(makeImage(env_.region.size()));  // nothing to draw: transparent
    planInputs();
    results_.assign(prims_.size(), std::nullopt);
    for (size_t i = 0; i < prims_.size(); ++i) {
      results_[i] = evaluate(i);
      release(i);
    }
    const Layer& last = *results_.back();
    return finish(inColorSpace(last.image, last.space, ColorSpace::SRGB));
  }

 private:
  Result finish(QImage image) const { return Result{std::move(image), env_.region, true}; }

  InputRef resolve(const QString& name, size_t at) const {
    using K = InputRef::Kind;
    if (name == QLatin1String("SourceGraphic")) return {K::SourceGraphic, -1};
    if (name == QLatin1String("SourceAlpha")) return {K::SourceAlpha, -1};
    // No BackgroundImage is kept, so both background inputs are transparent.
    if (name == QLatin1String("BackgroundImage") || name == QLatin1String("BackgroundAlpha")) return {K::Empty, -1};
    if (name == QLatin1String("FillPaint")) return {K::FillPaint, -1};
    if (name == QLatin1String("StrokePaint")) return {K::StrokePaint, -1};
    if (!name.isEmpty()) {
      // The nearest earlier primitive with this result name. A name that no
      // earlier primitive defines (forward reference, typo) falls back to the
      // previous result, as resvg and the Filter Effects text do.
      for (size_t j = at; j-- > 0;) {
        if (prims_[j]->result == name) return {K::Result, static_cast<int>(j)};
      }
    }
    return at == 0 ? InputRef{K::SourceGraphic, -1} : InputRef{K::Result, static_cast<int>(at - 1)};
  }

  void planInputs() {
    inputs_.assign(prims_.size(), {});
    lastUse_.assign(prims_.size(), -1);
    for (size_t i = 0; i < prims_.size(); ++i) {
      const Primitive& p = *prims_[i];
      PrimitiveInputs& in = inputs_[i];
      if (usesIn1(p.params)) in.in1 = resolve(p.in1, i);
      if (usesIn2(p.params)) in.in2 = resolve(std::get_if<Blend>(&p.params) ? std::get<Blend>(p.params).in2
                                              : std::get_if<Composite>(&p.params) ? std::get<Composite>(p.params).in2
                                                                                  : std::get<DisplacementMap>(p.params).in2,
                                              i);
      if (const auto* m = std::get_if<Merge>(&p.params)) {
        for (const QString& n : m->nodes) in.nodes.push_back(resolve(n, i));
      }
      auto mark = [&](const InputRef& r) {
        if (r.kind == InputRef::Kind::Result) lastUse_[static_cast<size_t>(r.index)] = static_cast<int>(i);
      };
      if (usesIn1(p.params)) mark(in.in1);
      if (usesIn2(p.params)) mark(in.in2);
      for (const InputRef& r : in.nodes) mark(r);
    }
  }

  // Drops results no later primitive reads. The last result stays for finish().
  void release(size_t i) {
    for (size_t j = 0; j <= i; ++j) {
      if (j + 1 < prims_.size() && results_[j] && lastUse_[j] <= static_cast<int>(i)) results_[j].reset();
    }
  }

  const Layer& input(const InputRef& r) {
    using K = InputRef::Kind;
    switch (r.kind) {
      case K::Result: return *results_[static_cast<size_t>(r.index)];
      case K::SourceGraphic:
        if (!sourceLayer_) sourceLayer_ = Layer{source_, full_, ColorSpace::SRGB};
        return *sourceLayer_;
      case K::SourceAlpha:
        if (!alphaLayer_) alphaLayer_ = Layer{alphaOf(source_), full_, ColorSpace::SRGB};
        return *alphaLayer_;
      case K::FillPaint: return paintLayer(fillLayer_, env_.context->fillPaint);
      case K::StrokePaint: return paintLayer(strokeLayer_, env_.context->strokePaint);
      case K::Empty: break;
    }
    if (!emptyLayer_) emptyLayer_ = Layer{makeImage(env_.region.size()), full_, ColorSpace::SRGB};
    return *emptyLayer_;
  }

  // FillPaint and StrokePaint: the element's flat paint color over the whole
  // filter region (SVG 1.1 "infinite tile"), or an empty image when the
  // element has none. resvg 0.50 passes SourceGraphic instead; this follows
  // the SVG 1.1 meaning for the one case the Context can express.
  const Layer& paintLayer(std::optional<Layer>& slot, const QColor& color) {
    if (!slot) {
      QImage image = makeImage(env_.region.size());
      if (color.isValid() && color.alpha() > 0) image.fill(premultipliedIn(color, 1.0, ColorSpace::SRGB));
      slot = Layer{image, full_, ColorSpace::SRGB};
    }
    return *slot;
  }

  static QImage alphaOf(const QImage& source) {
    QImage out = makeImage(source.size());
    for (int y = 0; y < source.height(); ++y) {
      const auto* s = reinterpret_cast<const QRgb*>(source.constScanLine(y));
      auto* d = reinterpret_cast<QRgb*>(out.scanLine(y));
      for (int x = 0; x < source.width(); ++x) d[x] = qRgba(0, 0, 0, qAlpha(s[x]));
    }
    return out;
  }

  Layer evaluate(size_t i) {
    const Primitive& p = *prims_[i];
    const PrimitiveInputs& refs = inputs_[i];
    const std::optional<QRectF> userSub = resolveSubregion(p.subregion, env_);
    QRect sub;
    if (userSub && userSub->width() > 0 && userSub->height() > 0) {
      sub = toDeviceRect(*userSub, env_.toDevice).translated(-env_.region.topLeft()).intersected(full_);
    }
    Layer out{makeImage(env_.region.size()), {}, p.colorSpace};
    if (sub.isEmpty()) {  // zero or negative size: the primitive yields transparent black
      out.subregion = QRect();
      return out;
    }
    const QRectF user = *userSub;
    const ColorSpace cs = p.colorSpace;
    const Layer none;  // never read: only primitives that use in1/in2 touch them
    const Layer& in1 = usesIn1(p.params) ? input(refs.in1) : none;
    const Layer& in2 = usesIn2(p.params) ? input(refs.in2) : none;
    out = std::visit(
        [&](const auto& params) -> Layer {
          using T = std::decay_t<decltype(params)>;
          if constexpr (std::is_same_v<T, Blend>) return applyBlend(env_, params, cs, sub, in1, in2);
          else if constexpr (std::is_same_v<T, ColorMatrix>) return applyColorMatrix(env_, params, cs, in1);
          else if constexpr (std::is_same_v<T, Composite>) return applyComposite(env_, params, cs, in1, in2);
          else if constexpr (std::is_same_v<T, Flood>) return applyFlood(env_, params, sub);
          else if constexpr (std::is_same_v<T, GaussianBlur>) return applyGaussianBlur(env_, params, cs, in1);
          else if constexpr (std::is_same_v<T, Merge>) {
            std::vector<const Layer*> nodes;
            for (const InputRef& r : refs.nodes) nodes.push_back(&input(r));
            return applyMerge(env_, cs, nodes);
          } else if constexpr (std::is_same_v<T, Offset>) return applyOffset(env_, params, in1);
          else if constexpr (std::is_same_v<T, Morphology>) return applyMorphology(env_, params, cs, in1);
          else if constexpr (std::is_same_v<T, Turbulence>) return applyTurbulence(env_, params, cs, sub, user);
          else if constexpr (std::is_same_v<T, DisplacementMap>) return applyDisplacementMap(env_, params, cs, sub, in1, in2);
          else if constexpr (std::is_same_v<T, ComponentTransfer>) return applyComponentTransfer(env_, params, cs, in1);
          else if constexpr (std::is_same_v<T, DropShadow>) return applyDropShadow(env_, params, cs, in1);
          else if constexpr (std::is_same_v<T, Image>) return applyImage(env_, params, sub, user);
          else if constexpr (std::is_same_v<T, ConvolveMatrix>) return applyConvolveMatrix(env_, params, cs, sub, in1);
          else if constexpr (std::is_same_v<T, Tile>) return applyTile(env_, sub, in1);
          else return applyLighting(env_, params, cs, sub, in1);  // Diffuse and Specular
        },
        p.params);
    if (out.image.isNull() || out.image.size() != env_.region.size()) out.image = makeImage(env_.region.size());
    // Each result is confined to its subregion.
    clipToRect(out.image, sub);
    out.subregion = sub;
    return out;
  }

  const Env& env_;
  std::vector<const Primitive*> prims_;
  QImage source_;
  QRect full_;
  std::vector<PrimitiveInputs> inputs_;
  std::vector<int> lastUse_;
  std::vector<std::optional<Layer>> results_;
  std::optional<Layer> sourceLayer_, alphaLayer_, emptyLayer_, fillLayer_, strokeLayer_;
};

}  // namespace
}  // namespace detail

QRect regionInDevice(const Definition& definition, const Context& context) {
  const auto env = detail::makeEnv(definition, context);
  return env ? env->region : QRect();
}

Result apply(const Definition& definition, const Context& context, const QImage& source) {
  const auto env = detail::makeEnv(definition, context);
  if (!env) return {};
  std::vector<const Primitive*> primitives;
  for (const Primitive* p : definition.primitives) {
    if (p) primitives.push_back(p);
  }
  return detail::Evaluator(*env, std::move(primitives), source).run();
}

}  // namespace rnqnx::views::rnsvg::filter
