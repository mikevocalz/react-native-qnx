#pragma once

#include <QFont>
#include <QString>

#include <react/renderer/attributedstring/AttributedString.h>
#include <react/renderer/attributedstring/TextAttributes.h>

namespace rnqnx::text {

/// Resolves an RN font in Qt logical pixels. Measurement and painting must use
/// this same value; display DPR is applied by Qt, never by this conversion.
QFont fontFromTextAttributes(const facebook::react::TextAttributes& attributes);

/// Explicit line height after RN accessibility scaling; zero means natural
/// font metrics. Qt Quick has one line-height property per paragraph, so mixed
/// fragments use the largest requested height consistently in both paths.
qreal paragraphLineHeight(const facebook::react::AttributedString& attributedString);

/// Escaped markup generated only from RN text and style attributes. This is
/// Qt Quick's existing rich-text path, not an application HTML API.
QString fragmentMarkup(const facebook::react::AttributedString::Fragment& fragment);

}  // namespace rnqnx::text
