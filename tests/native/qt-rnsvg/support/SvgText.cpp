#include "SvgText.h"

#include "PropBag.h"
#include "SvgCommonProps.h"
#include "SvgValues.h"

namespace rnqnx::views::rnsvg::testing {

TextChildPlan planTextChildren(const XmlNode& node, bool container) {
  TextChildPlan plan;
  if (node.children.size() == 1) {
    if (const auto* s = std::get_if<QString>(&node.children[0])) {
      if (container) plan.children.push_back({nullptr, *s});
      else plan.content = *s;
      return plan;
    }
  }
  for (const auto& child : node.children) {
    if (const auto* s = std::get_if<QString>(&child)) plan.children.push_back({nullptr, *s});
    else plan.children.push_back({std::get<std::unique_ptr<XmlNode>>(child).get(), {}});
  }
  return plan;
}

ElementProps buildTextElement(Kind kind, const XmlNode& node) {
  const PropBag p(node);
  ElementProps e;
  e.kind = kind;
  extractCommonProps(e, p);
  const bool textPath = kind != Kind::Text && kind != Kind::TSpan;
  if (textPath) {
#if RNQNX_RNSVG_FULL_MODEL
    // TextPath.tsx builds props with extractText({children}) only: no font and
    // no per-character lists of its own.
    e.textPath.href = parseUrlId(p.has(QStringLiteral("href")) ? p.get(QStringLiteral("href")) : p.get(QStringLiteral("xlinkHref")));
    e.textPath.side = p.get(QStringLiteral("side"));
    e.textPath.method = p.get(QStringLiteral("method"));
    e.textPath.spacing = p.get(QStringLiteral("spacing"));
    e.textPath.midLine = p.get(QStringLiteral("midLine"));
    e.textPath.startOffset = p.has(QStringLiteral("startOffset")) ? parseLength(p.get(QStringLiteral("startOffset"))) : lengthPx(0);
    e.text.alignmentBaseline = p.get(QStringLiteral("alignmentBaseline"));
#endif
    return e;
  }
  e.font = extractFont(p);
  auto list = [&](const char* name) { return parseLengthList(p.get(QLatin1String(name))); };
  setTextPositions(e, list("x"), list("y"), list("dx"), list("dy"));
#if RNQNX_RNSVG_FULL_MODEL
  e.text.rotate = parseNumberList(p.get(QStringLiteral("rotate")));
  e.text.inlineSize = lengthProp(p, "inlineSize");
  e.text.verticalAlign = lengthProp(p, "verticalAlign");
  e.text.alignmentBaseline = p.get(QStringLiteral("alignmentBaseline"));
  const QString shift = p.get(QStringLiteral("baselineShift"));
  if (shift == QLatin1String("baseline") || shift == QLatin1String("sub") || shift == QLatin1String("super")) e.text.baselineShiftKeyword = shift;
  else e.text.baselineShift = parseLength(shift);
#endif
  return e;
}

}  // namespace rnqnx::views::rnsvg::testing
