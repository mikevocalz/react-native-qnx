// Table rows for Text, TSpan and TextPath.

#include "QtRnsvgBridgeInternal.h"

#include <react/renderer/components/rnsvg/Props.h>

namespace rnqnx::fabric::rnsvg_bridge {
namespace {

template <class P>
void textElementCommon(ElementProps& e, const P& p) {
  renderable(e, p);
  fontCommon(e, p);
  textCommon(e, p);
}

bool convertText(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGTextProps>(props, e, previous, [](const rn::RNSVGTextProps& p, ElementProps& e, const ElementProps&) {
    textElementCommon(e, p);
  });
}

bool convertTSpan(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGTSpanProps>(props, e, previous, [](const rn::RNSVGTSpanProps& p, ElementProps& e, const ElementProps&) {
    textElementCommon(e, p);
    e.content = QString::fromStdString(p.content);
  });
}

bool convertTextPath(const rn::Props& props, ElementProps& e, const ElementProps& previous) {
  return convertAs<rn::RNSVGTextPathProps>(props, e, previous, [](const rn::RNSVGTextPathProps& p, ElementProps& e, const ElementProps&) {
    textElementCommon(e, p);
    TextPathProps& tp = e.textPath;
    tp.href = QString::fromStdString(p.href);  // the id, `#` already stripped by TextPath.tsx
    tp.side = QString::fromStdString(p.side);
    tp.method = QString::fromStdString(p.method);
    tp.midLine = QString::fromStdString(p.midLine);
    tp.spacing = QString::fromStdString(p.spacing);
    tp.startOffset = toLength(p.startOffset);
  });
}

}  // namespace

void appendTextEntries(std::vector<Entry>& out) {
  out.push_back(Entry{"RNSVGText", Kind::Text, &convertText, true});
  out.push_back(Entry{"RNSVGTSpan", Kind::TSpan, &convertTSpan, true});
  out.push_back(Entry{"RNSVGTextPath", Kind::TextPath, &convertTextPath, true});
}

}  // namespace rnqnx::fabric::rnsvg_bridge
