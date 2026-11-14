#pragma once

#include "ModelAdapter.h"
#include "XmlDocument.h"

#include <vector>

namespace rnqnx::views::rnsvg::testing {

// Text, TSpan and TextPath props: font, position lists, content and the
// TextPath path reference (extractText() in src/lib/extract/extractText.tsx).
ElementProps buildTextElement(Kind kind, const XmlNode& node);

// What extractText() does with an element's children. A single string child
// becomes the element's own `content` for a TSpan and a TSpan child for Text
// and TextPath; several children, or one element, become child elements with
// each bare string wrapped in a TSpan.
struct TextChild {
  const XmlNode* element{nullptr};  // null for a bare string
  QString text;                     // the string, when `element` is null
};
struct TextChildPlan {
  QString content;                  // the element's own text (TSpan with one string child)
  std::vector<TextChild> children;  // in document order
};
TextChildPlan planTextChildren(const XmlNode& node, bool container);

}  // namespace rnqnx::views::rnsvg::testing
