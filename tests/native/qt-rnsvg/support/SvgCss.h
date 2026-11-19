#pragma once

// <style> handling as react-native-svg's SvgCss does it (src/css/css.tsx):
// rules are matched against the element tree and their declarations are merged
// into each element's `style`, where an inline declaration already present is
// kept unless the rule says !important. SvgXml does not do this; the loader
// applies it only to fixtures the manifest marks `css`.

#include "XmlDocument.h"

namespace rnqnx::views::rnsvg::testing {

// Supported selectors: type, .class, #id, `*`, compounds of those, descendant
// and child combinators, and comma lists. Rules with pseudo-classes, attribute
// selectors or at-rules are skipped, as css.tsx skips the ones it cannot use.
void applyStyleSheets(XmlNode& root);

}  // namespace rnqnx::views::rnsvg::testing
