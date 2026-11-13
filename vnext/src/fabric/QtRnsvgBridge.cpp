#include "react-native-qnx/fabric/QtRnsvgBridge.h"

#include <RnsvgItems.h>
#include <RnsvgPath.h>

#include <folly/dynamic.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/components/rnsvg/Props.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/mounting/ShadowView.h>

#include <react/renderer/components/view/ConcreteViewShadowNode.h>

#include "RNSVGComponentDescriptors.h"
#include "RNSVGConcreteShadowNode.h"
#include "RNSVGShadowNodes.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QQmlEngine>
#include <QUrl>

#include <cmath>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace facebook::react {
// Names for the shadow nodes this host mounts. The package defines these in
// RNSVGShadowNodes.cpp, which includes a generated header this host does not
// produce; the strings are the Fabric component names.
extern const char RNSVGCircleComponentName[] = "RNSVGCircle";
extern const char RNSVGClipPathComponentName[] = "RNSVGClipPath";
extern const char RNSVGDefsComponentName[] = "RNSVGDefs";
extern const char RNSVGEllipseComponentName[] = "RNSVGEllipse";
extern const char RNSVGGroupComponentName[] = "RNSVGGroup";
extern const char RNSVGLinearGradientComponentName[] = "RNSVGLinearGradient";
extern const char RNSVGLineComponentName[] = "RNSVGLine";
extern const char RNSVGMaskComponentName[] = "RNSVGMask";
extern const char RNSVGPathComponentName[] = "RNSVGPath";
extern const char RNSVGRadialGradientComponentName[] = "RNSVGRadialGradient";
extern const char RNSVGRectComponentName[] = "RNSVGRect";
extern const char RNSVGTextComponentName[] = "RNSVGText";
extern const char RNSVGTSpanComponentName[] = "RNSVGTSpan";
extern const char RNSVGUseComponentName[] = "RNSVGUse";
// react-native-svg's own Image shadow node needs an ImageManager and a state
// round trip this renderer does not use (it decodes the source itself), so the
// plain element shadow node is used for RNSVGImage.
constexpr char RNSVGImageComponentName[] = "RNSVGImage";
using RNSVGImageShadowNode = RNSVGConcreteShadowNode<RNSVGImageComponentName, RNSVGImageProps>;
// <Svg> takes its size from ordinary Yoga style, unlike its elements. It must
// still be mounted (not flattened) so its Qt item exists to hold the tree.
constexpr char RNSVGSvgViewComponentName[] = "RNSVGSvgView";
class RNSVGSvgViewShadowNode final
    : public ConcreteViewShadowNode<RNSVGSvgViewComponentName, RNSVGSvgViewProps, ViewEventEmitter> {
 public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;
  static ShadowNodeTraits BaseTraits() {
    auto traits = ConcreteViewShadowNode::BaseTraits();
    traits.set(ShadowNodeTraits::Trait::FormsView);
    traits.set(ShadowNodeTraits::Trait::FormsStackingContext);
    return traits;
  }
};
}  // namespace facebook::react

namespace rnqnx::fabric {
namespace {

using namespace rnqnx::views::rnsvg;
namespace rn = facebook::react;

// ---- value conversion -----------------------------------------------------

Length lengthFromString(const std::string& raw) {
  const char* begin = raw.c_str();
  while (*begin == ' ') ++begin;
  char* end = nullptr;
  const double v = std::strtod(begin, &end);
  if (end == begin || !std::isfinite(v)) return {};
  std::string_view unit(end);
  while (!unit.empty() && unit.front() == ' ') unit.remove_prefix(1);
  while (!unit.empty() && unit.back() == ' ') unit.remove_suffix(1);
  if (unit == "%") return Length{v, Length::Unit::Percent, true};
  if (unit == "em") return Length{v, Length::Unit::Em, true};
  double scale = 1;
  if (unit == "pt") scale = 4.0 / 3.0;
  else if (unit == "pc") scale = 16;
  else if (unit == "mm") scale = 96.0 / 25.4;
  else if (unit == "cm") scale = 96.0 / 2.54;
  else if (unit == "in") scale = 96;
  return Length{v * scale, Length::Unit::Px, true};
}

Length toLength(const folly::dynamic& d) {
  if (d.isNumber()) return d.isDouble() || d.isInt() ? Length{d.asDouble(), Length::Unit::Px, true} : Length{};
  if (d.isString()) return lengthFromString(d.getString());
  return {};
}

std::vector<Length> toLengths(const folly::dynamic& d) {
  std::vector<Length> out;
  if (d.isArray()) {
    for (const auto& item : d) out.push_back(toLength(item));
  } else if (d.isNumber() || d.isString()) {
    out.push_back(toLength(d));
  }
  return out;
}

QColor colorFromArgb(double packed) {
  // JavaScript hands over a signed 32-bit integer; reinterpret as ARGB.
  const auto bits = static_cast<std::uint32_t>(static_cast<std::int64_t>(packed));
  return QColor::fromRgba(bits);
}

QColor colorFrom(const rn::SharedColor& c) {
  return QColor(rn::redFromColor(c), rn::greenFromColor(c), rn::blueFromColor(c), rn::alphaFromColor(c));
}

Brush toBrush(const folly::dynamic& d) {
  Brush b;
  if (d.isNumber()) {
    b.type = Brush::Type::Color;
    b.color = colorFromArgb(d.asDouble());
    return b;
  }
  if (!d.isObject()) return b;  // null: `none`
  int kind = -1;
  if (const auto* v = d.get_ptr("type"); v && v->isNumber()) kind = static_cast<int>(v->asInt());
  switch (kind) {
    case 0: {
      b.type = Brush::Type::Color;
      b.color = QColor(Qt::black);
      if (const auto* p = d.get_ptr("payload"); p && p->isNumber()) b.color = colorFromArgb(p->asDouble());
      break;
    }
    case 1:
      b.type = Brush::Type::Ref;
      if (const auto* r = d.get_ptr("brushRef"); r && r->isString()) b.ref = QString::fromStdString(r->getString());
      break;
    case 2: b.type = Brush::Type::CurrentColor; break;
    case 3: b.type = Brush::Type::ContextFill; break;
    case 4: b.type = Brush::Type::ContextStroke; break;
    default: break;
  }
  return b;
}

std::optional<int> toWeight(const folly::dynamic& d) {
  if (d.isNumber()) return static_cast<int>(d.asDouble());
  if (!d.isString()) return std::nullopt;
  const auto& s = d.getString();
  if (s == "bold" || s == "bolder") return 700;
  if (s == "normal") return 400;
  if (s == "lighter") return 300;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str()) return static_cast<int>(v);
  return std::nullopt;
}

void mergeFont(FontSpec& f, const folly::dynamic& font) {
  if (!font.isObject()) return;
  if (const auto* v = font.get_ptr("fontFamily"); v && v->isString()) f.family = QString::fromStdString(v->getString());
  if (const auto* v = font.get_ptr("fontSize"); v) {
    const Length l = toLength(*v);
    if (l.set && l.unit != Length::Unit::Percent) f.size = l.unit == Length::Unit::Em ? l.value * 12.0 : l.value;
  }
  if (const auto* v = font.get_ptr("fontWeight"); v) {
    if (auto w = toWeight(*v)) f.weight = *w;
  }
  if (const auto* v = font.get_ptr("fontStyle"); v && v->isString()) f.italic = v->getString() == "italic" || v->getString() == "oblique";
  if (const auto* v = font.get_ptr("textAnchor"); v && v->isString()) f.anchor = QString::fromStdString(v->getString());
  if (const auto* v = font.get_ptr("letterSpacing"); v) {
    const Length l = toLength(*v);
    if (l.set) f.letterSpacing = l.value;
  }
  if (const auto* v = font.get_ptr("textDecoration"); v && v->isString()) f.decoration = QString::fromStdString(v->getString());
}

QTransform toTransform(const std::vector<rn::Float>& m) {
  return QTransform(m[0], m[1], m[2], m[3], m[4], m[5]);
}

QImage loadImage(const std::string& uri) {
  if (uri.empty()) return {};
  const QString u = QString::fromStdString(uri);
  if (u.startsWith(QStringLiteral("data:"))) {
    const int comma = u.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QByteArray payload = u.mid(comma + 1).toUtf8();
    const bool base64 = u.left(comma).contains(QStringLiteral(";base64"));
    return QImage::fromData(base64 ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload));
  }
  // Bundled assets and remote URLs go through the Image component's loader,
  // which this renderer does not call; local files are read directly.
  if (u.startsWith(QStringLiteral("file://"))) return QImage(QUrl(u).toLocalFile());
  if (u.startsWith(QLatin1Char('/'))) return QImage(u);
  return {};
}

// ---- props -> retained element props -------------------------------------

template <class P>
void nodeCommon(ElementProps& e, const P& p) {
  e.name = QString::fromStdString(p.name);
  e.opacity = p.opacity;
  if (p.matrix.size() >= 6) e.matrix = toTransform(p.matrix);
  e.clipPath = QString::fromStdString(p.clipPath);
  e.mask = QString::fromStdString(p.mask);
  e.clipEvenOdd = false;  // see RnsvgRenderer.cpp: clipRule 0 is also the unset default
  e.responsible = p.responsible;
  e.displayNone = p.display == "none";
  e.pointerEvents = QString::fromStdString(p.pointerEvents);
}

template <class P>
void renderable(ElementProps& e, const P& p) {
  nodeCommon(e, p);
  if (rn::isColorMeaningful(p.color)) e.color = colorFrom(p.color);
  e.fill = toBrush(p.fill);
  e.stroke = toBrush(p.stroke);
  e.fillOpacity = p.fillOpacity;
  e.strokeOpacity = p.strokeOpacity;
  e.fillEvenOdd = p.fillRule == 0;
  e.strokeWidth = toLength(p.strokeWidth);
  e.strokeLinecap = p.strokeLinecap;
  e.strokeLinejoin = p.strokeLinejoin;
  e.strokeDasharray = toLengths(p.strokeDasharray);
  e.strokeDashoffset = p.strokeDashoffset;
  e.strokeMiterlimit = p.strokeMiterlimit;
  e.nonScalingStroke = p.vectorEffect == 1;
  for (const auto& name : p.propList) e.propList.insert(QString::fromStdString(name));
}

template <class P>
void textCommon(ElementProps& e, const P& p) {
  mergeFont(e.font, p.font);
  // fontSize and fontWeight can also arrive as their own props.
  if (!p.fontSize.isNull()) {
    const Length l = toLength(p.fontSize);
    if (l.set && l.unit != Length::Unit::Percent) e.font.size = l.value;
  }
  if (!p.fontWeight.isNull()) {
    if (auto w = toWeight(p.fontWeight)) e.font.weight = *w;
  }
}

template <class P>
void gradientCommon(ElementProps& e, const P& p) {
  nodeCommon(e, p);
  e.gradientUserSpace = p.gradientUnits == 1;
  if (p.gradientTransform.size() >= 6) e.gradientTransform = toTransform(p.gradientTransform);
  for (size_t i = 0; i + 1 < p.gradient.size(); i += 2) {
    e.stops.push_back({p.gradient[i], colorFromArgb(p.gradient[i + 1])});
  }
}

using Converter = bool (*)(const rn::Props&, ElementProps&, const ElementProps& previous);

template <class P, class Fn>
bool convert(const rn::Props& props, ElementProps& e, const ElementProps& previous, Fn&& fn) {
  const auto* p = dynamic_cast<const P*>(&props);
  if (!p) return false;
  fn(*p, e, previous);
  return true;
}

struct Entry {
  Kind kind;
  Converter convert;
};

const std::unordered_map<std::string, Entry>& table() {
  static const std::unordered_map<std::string, Entry> t = {
      {"RNSVGGroup", {Kind::Group, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGGroupProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           textCommon(e, p);
         });
       }}},
      {"RNSVGDefs", {Kind::Defs, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGDefsProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) { nodeCommon(e, p); });
       }}},
      {"RNSVGPath", {Kind::Path, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGPathProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           e.path = parseSvgPath(QString::fromStdString(p.d));
         });
       }}},
      {"RNSVGCircle", {Kind::Circle, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGCircleProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           e.cx = toLength(p.cx); e.cy = toLength(p.cy); e.r = toLength(p.r);
         });
       }}},
      {"RNSVGEllipse", {Kind::Ellipse, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGEllipseProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           e.cx = toLength(p.cx); e.cy = toLength(p.cy); e.rx = toLength(p.rx); e.ry = toLength(p.ry);
         });
       }}},
      {"RNSVGRect", {Kind::Rect, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGRectProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           e.x = toLength(p.x); e.y = toLength(p.y); e.width = toLength(p.width); e.height = toLength(p.height);
           e.rx = toLength(p.rx); e.ry = toLength(p.ry);
         });
       }}},
      {"RNSVGLine", {Kind::Line, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGLineProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           e.x1 = toLength(p.x1); e.y1 = toLength(p.y1); e.x2 = toLength(p.x2); e.y2 = toLength(p.y2);
         });
       }}},
      {"RNSVGLinearGradient", {Kind::LinearGradient, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGLinearGradientProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           gradientCommon(e, p);
           e.x1 = toLength(p.x1); e.y1 = toLength(p.y1); e.x2 = toLength(p.x2); e.y2 = toLength(p.y2);
         });
       }}},
      {"RNSVGRadialGradient", {Kind::RadialGradient, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGRadialGradientProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           gradientCommon(e, p);
           e.fx = toLength(p.fx); e.fy = toLength(p.fy); e.cx = toLength(p.cx); e.cy = toLength(p.cy);
           e.rx = toLength(p.rx); e.ry = toLength(p.ry);
         });
       }}},
      {"RNSVGClipPath", {Kind::ClipPath, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGClipPathProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           textCommon(e, p);
         });
       }}},
      {"RNSVGMask", {Kind::Mask, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGMaskProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           textCommon(e, p);
           e.x = toLength(p.x); e.y = toLength(p.y); e.width = toLength(p.width); e.height = toLength(p.height);
           e.maskUserSpace = p.maskUnits == 1;
           e.maskContentUserSpace = p.maskContentUnits != 0;
           e.maskAlpha = p.maskType == 1;
         });
       }}},
      {"RNSVGText", {Kind::Text, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGTextProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           textCommon(e, p);
           e.tx = toLengths(p.x); e.ty = toLengths(p.y); e.tdx = toLengths(p.dx); e.tdy = toLengths(p.dy);
         });
       }}},
      {"RNSVGTSpan", {Kind::TSpan, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGTSpanProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           textCommon(e, p);
           e.tx = toLengths(p.x); e.ty = toLengths(p.y); e.tdx = toLengths(p.dx); e.tdy = toLengths(p.dy);
           e.content = QString::fromStdString(p.content);
         });
       }}},
      {"RNSVGUse", {Kind::Use, [](const rn::Props& props, ElementProps& e, const ElementProps&) {
         return convert<rn::RNSVGUseProps>(props, e, e, [](const auto& p, ElementProps& e, const ElementProps&) {
           renderable(e, p);
           e.href = QString::fromStdString(p.href);
           e.x = toLength(p.x); e.y = toLength(p.y); e.width = toLength(p.width); e.height = toLength(p.height);
         });
       }}},
      {"RNSVGImage", {Kind::Image, [](const rn::Props& props, ElementProps& e, const ElementProps& previous) {
         return convert<rn::RNSVGImageProps>(props, e, previous, [](const auto& p, ElementProps& e, const ElementProps& previous) {
           renderable(e, p);
           e.x = toLength(p.x); e.y = toLength(p.y); e.width = toLength(p.width); e.height = toLength(p.height);
           e.imageUri = QString::fromStdString(p.src.uri);
           // Decode once per source, not once per commit.
           e.image = e.imageUri == previous.imageUri ? previous.image : loadImage(p.src.uri);
           e.imageAlign = QString::fromStdString(p.align.empty() ? std::string("xMidYMid") : p.align);
           e.imageMeetOrSlice = p.meetOrSlice;
         });
       }}},
  };
  return t;
}

}  // namespace

std::vector<std::string> rnsvgComponentNames() {
  std::vector<std::string> names{"RNSVGSvgView"};
  for (const auto& [name, entry] : table()) names.push_back(name);
  return names;
}

bool isRnsvgComponent(const std::string& name) {
  return name == "RNSVGSvgView" || table().count(name) != 0;
}

void addRnsvgComponentDescriptors(rn::ComponentDescriptorProviderRegistry& registry) {
  using namespace rn;
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGSvgViewShadowNode>>());
  registry.add(concreteComponentDescriptorProvider<RNSVGGroupComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGDefsComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGPathComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGCircleComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGEllipseComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGRectComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGLineComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGLinearGradientComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGRadialGradientComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGClipPathComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGMaskComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGTextComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGTSpanComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<RNSVGUseComponentDescriptor>());
  registry.add(concreteComponentDescriptorProvider<ConcreteComponentDescriptor<RNSVGImageShadowNode>>());
}

bool applyRnsvgProps(QQuickItem* item, const rn::ShadowView& view) {
  if (view.componentName == nullptr || !view.props) return false;
  const std::string name(view.componentName);
  if (name == "RNSVGSvgView") {
    auto* root = qobject_cast<views::RnsvgRootItem*>(item);
    const auto* p = dynamic_cast<const rn::RNSVGSvgViewProps*>(view.props.get());
    if (!root || !p) return false;
    RootProps rp;
    rp.bbWidth = toLength(p->bbWidth);
    rp.bbHeight = toLength(p->bbHeight);
    rp.minX = p->minX; rp.minY = p->minY; rp.vbWidth = p->vbWidth; rp.vbHeight = p->vbHeight;
    if (!p->align.empty()) rp.align = QString::fromStdString(p->align);
    rp.meetOrSlice = p->meetOrSlice;
    if (rn::isColorMeaningful(p->color)) rp.color = colorFrom(p->color);
    root->setRootProps(rp);
    return true;
  }
  const auto it = table().find(name);
  auto* element = qobject_cast<views::RnsvgElementItem*>(item);
  if (it == table().end() || !element) return false;
  ElementProps next;
  next.kind = it->second.kind;
  if (!it->second.convert(*view.props, next, element->props())) return false;
  element->setProps(std::move(next));
  return true;
}

}  // namespace rnqnx::fabric
