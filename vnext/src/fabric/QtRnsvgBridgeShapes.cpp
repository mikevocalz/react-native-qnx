// Table rows for the containers and basic shapes: Group, Defs, Path, Circle,
// Ellipse, Rect, Line, Use and Image (the converter comes from the image bridge).

#include "QtRnsvgBridgeInternal.h"
#include "react-native-qnx/fabric/QtRnsvgImageBridge.h"

#include <RnsvgPath.h>

#include <react/renderer/components/rnsvg/Props.h>

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

// A row is a name, the model kind, a converter and whether the component has
// an onSvgLayout event. The converter writes `e` from the props; `previous`
// is the element's retained props, for converters that skip repeated work.
template <class P, class Fn>
Entry row(const char* name, Kind kind, bool emitsSvgLayout, Fn) {
  return Entry{name, kind,
               [](const rn::Props& props, ElementProps& e, const ElementProps& previous) {
                 return convertAs<P>(props, e, previous, Fn{});
               },
               emitsSvgLayout};
}

struct GroupFn {
  void operator()(const rn::RNSVGGroupProps& p, ElementProps& e, const ElementProps&) const {
    renderable(e, p);
    fontCommon(e, p);
  }
};
struct DefsFn {
  void operator()(const rn::RNSVGDefsProps& p, ElementProps& e, const ElementProps&) const { nodeCommon(e, p); }
};
struct PathFn {
  void operator()(const rn::RNSVGPathProps& p, ElementProps& e, const ElementProps& previous) const {
    renderable(e, p);
    e.pathData = QString::fromStdString(p.d);
    if (e.pathData == previous.pathData && previous.kind == Kind::Path) {
      e.path = previous.path;
      e.vertices = previous.vertices;
    } else {
      e.path = parseSvgPath(e.pathData, nullptr, &e.vertices);
    }
  }
};
struct CircleFn {
  void operator()(const rn::RNSVGCircleProps& p, ElementProps& e, const ElementProps&) const {
    renderable(e, p);
    e.cx = toLength(p.cx);
    e.cy = toLength(p.cy);
    e.r = toLength(p.r);
  }
};
struct EllipseFn {
  void operator()(const rn::RNSVGEllipseProps& p, ElementProps& e, const ElementProps&) const {
    renderable(e, p);
    e.cx = toLength(p.cx);
    e.cy = toLength(p.cy);
    e.rx = toLength(p.rx);
    e.ry = toLength(p.ry);
  }
};
struct RectFn {
  void operator()(const rn::RNSVGRectProps& p, ElementProps& e, const ElementProps&) const {
    renderable(e, p);
    e.x = toLength(p.x);
    e.y = toLength(p.y);
    e.width = toLength(p.width);
    e.height = toLength(p.height);
    e.rx = toLength(p.rx);
    e.ry = toLength(p.ry);
  }
};
struct LineFn {
  void operator()(const rn::RNSVGLineProps& p, ElementProps& e, const ElementProps&) const {
    renderable(e, p);
    e.x1 = toLength(p.x1);
    e.y1 = toLength(p.y1);
    e.x2 = toLength(p.x2);
    e.y2 = toLength(p.y2);
  }
};
struct UseFn {
  void operator()(const rn::RNSVGUseProps& p, ElementProps& e, const ElementProps&) const {
    renderable(e, p);
    e.href = QString::fromStdString(p.href);
    e.x = toLength(p.x);
    e.y = toLength(p.y);
    e.width = toLength(p.width);
    e.height = toLength(p.height);
  }
};

}  // namespace

void appendShapeEntries(std::vector<Entry>& out) {
  out.push_back(row<rn::RNSVGGroupProps>("RNSVGGroup", Kind::Group, true, GroupFn{}));
  out.push_back(row<rn::RNSVGDefsProps>("RNSVGDefs", Kind::Defs, false, DefsFn{}));
  out.push_back(row<rn::RNSVGPathProps>("RNSVGPath", Kind::Path, true, PathFn{}));
  out.push_back(row<rn::RNSVGCircleProps>("RNSVGCircle", Kind::Circle, true, CircleFn{}));
  out.push_back(row<rn::RNSVGEllipseProps>("RNSVGEllipse", Kind::Ellipse, true, EllipseFn{}));
  out.push_back(row<rn::RNSVGRectProps>("RNSVGRect", Kind::Rect, true, RectFn{}));
  out.push_back(row<rn::RNSVGLineProps>("RNSVGLine", Kind::Line, true, LineFn{}));
  out.push_back(row<rn::RNSVGUseProps>("RNSVGUse", Kind::Use, true, UseFn{}));
  // Image has its own bridge (QtRnsvgImageBridge): the converter here, the
  // load and onLoad in QtRnsvgBridge.cpp's applyRnsvgProps.
  out.push_back(Entry{"RNSVGImage", Kind::Image, &convertImageProps, true});
}

}  // namespace rnqnx::fabric::rnsvg_bridge
