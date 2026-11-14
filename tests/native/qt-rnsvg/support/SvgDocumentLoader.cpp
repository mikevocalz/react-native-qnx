#include "SvgDocumentLoader.h"

#include "PropBag.h"
#include "SvgCommonProps.h"
#include "SvgCss.h"
#include "SvgElementBuilders.h"
#include "SvgFilters.h"
#include "SvgText.h"
#include "SvgValues.h"

#include <QSet>

namespace rnqnx::views::rnsvg::testing {
namespace {

constexpr int kMaxElements = 500000;

// Tags that never produce an element and are not worth reporting.
bool isSilentTag(const QString& tag) {
  static const QSet<QString> silent = {"style", "title", "desc", "metadata", "stop", "feMergeNode", "feDistantLight", "fePointLight",
                                       "feSpotLight", "feFuncR", "feFuncG", "feFuncB", "feFuncA"};
  return silent.contains(tag);
}

class Builder {
 public:
  explicit Builder(LoadedDocument& doc) : doc_(doc) {}

  void addChildren(const XmlNode& node, QQuickItem* parent, int depth) {
    for (const auto& child : node.children) {
      if (const auto* element = std::get_if<std::unique_ptr<XmlNode>>(&child)) addElement(**element, parent, depth + 1);
    }
  }

 private:
  RnsvgElementItem* make(ElementProps props, QQuickItem* parent) {
    const Kind kind = props.kind;
    auto* item = new RnsvgElementItem(kind, parent);
    item->setProps(std::move(props));
    ++count_;
    return item;
  }

  void addElement(const XmlNode& node, QQuickItem* parent, int depth) {
    if (depth > kMaxXmlDepth || count_ >= kMaxElements) return;
    if (isFilterPrimitiveTag(node.tag)) {
      auto primitives = buildFilterPrimitives(node);
      if (primitives.empty() && !isSilentTag(node.tag)) skip(node.tag);
      for (auto& p : primitives) make(std::move(p), parent);
      return;
    }
    const auto kind = kindForTag(node.tag);
    if (!kind) {
      if (!isSilentTag(node.tag)) skip(node.tag);
      return;
    }
    if (node.tag == QLatin1String("svg")) skip(QStringLiteral("svg(nested)"));
    ElementProps props = buildElementProps(*kind, node);
    const bool text = *kind == Kind::Text || *kind == Kind::TSpan;
    const bool textPath = !text && node.tag == QLatin1String("textPath");
    TextChildPlan plan;
    if (text || textPath) {
      plan = planTextChildren(node, /*container=*/*kind != Kind::TSpan);
      props.content = plan.content;
    }
    RnsvgElementItem* item = make(std::move(props), parent);
    if (text || textPath) {
      for (const TextChild& child : plan.children) {
        if (child.element) {
          addElement(*child.element, item, depth + 1);
        } else {
          ElementProps span;
          span.kind = Kind::TSpan;
          span.fill = Brush{Brush::Type::Color, QColor(Qt::black), {}};
          span.content = child.text;
          make(std::move(span), item);
        }
      }
    } else if (*kind != Kind::LinearGradient && *kind != Kind::RadialGradient) {
      addChildren(node, item, depth);
    }
  }

  void skip(const QString& tag) {
    if (!doc_.skipped.contains(tag)) doc_.skipped.push_back(tag);
  }

  LoadedDocument& doc_;
  int count_{0};
};

// The props <Svg> forwards to its inner <G>: inherited paint props and `font`.
const QSet<QString>& rootGroupProps() {
  static const QSet<QString> names = {"font", "fill", "fillOpacity", "fillRule", "stroke", "strokeWidth", "strokeOpacity",
                                      "strokeDasharray", "strokeDashoffset", "strokeLinecap", "strokeLinejoin", "strokeMiterlimit"};
  return names;
}

}  // namespace

LoadedDocument loadSvgDocument(const QByteArray& xml, const LoadOptions& options) {
  LoadedDocument doc;
  XmlParseResult parsed = parseXml(xml);
  if (!parsed.root) {
    doc.error = parsed.error.isEmpty() ? QStringLiteral("empty document") : parsed.error;
    return doc;
  }
  XmlNode& svg = *parsed.root;
  if (svg.tag != QLatin1String("svg")) {
    doc.error = QStringLiteral("root element is <%1>, not <svg>").arg(svg.tag);
    return doc;
  }
  if (options.applyStyleSheets) applyStyleSheets(svg);

  const PropBag rootBag(svg);
  auto root = std::make_unique<RnsvgRootItem>();

  RootProps rp;
  rp.bbWidth = lengthProp(rootBag, "width");
  rp.bbHeight = lengthProp(rootBag, "height");
  const ParsedViewBox vb = parseViewBox(rootBag.get(QStringLiteral("viewBox")),
                                        rootBag.has(QStringLiteral("preserveAspectRatio")) ? rootBag.get(QStringLiteral("preserveAspectRatio")) : QStringLiteral("xMidYMid meet"));
  if (vb.set) {
    rp.minX = vb.minX; rp.minY = vb.minY; rp.vbWidth = vb.width; rp.vbHeight = vb.height;
    rp.align = vb.align; rp.meetOrSlice = vb.meetOrSlice;
  }
  if (rootBag.has(QStringLiteral("color"))) {
    if (const auto c = parseColor(rootBag.get(QStringLiteral("color")))) rp.color = *c;
  }
  if (rootBag.has(QStringLiteral("opacity"))) doc.rootOpacity = parseOpacity(rootBag.get(QStringLiteral("opacity")));

  root->setSize(options.itemSize);
  root->setRootProps(rp);

  // <Svg> mounts one inner <G> carrying the inherited props.
  ElementProps groupProps;
  groupProps.kind = Kind::Group;
  extractCommonProps(groupProps, PropBag(svg, &rootGroupProps()));
  groupProps.font = extractFont(PropBag(svg, &rootGroupProps()));
  auto* group = new RnsvgElementItem(Kind::Group, root.get());
  group->setProps(std::move(groupProps));

  Builder(doc).addChildren(svg, group, 0);
  doc.root = std::move(root);
  return doc;
}

}  // namespace rnqnx::views::rnsvg::testing
