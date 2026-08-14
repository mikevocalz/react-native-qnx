// This platform's TextLayoutManager, replacing React Native's generic C++ one.
//
// React Native selects a TextLayoutManager implementation per platform; the
// generic `platform/cxx` version does not measure at all — it returns
// `layoutConstraints.minimumSize`
// (textlayoutmanager/platform/cxx/.../TextLayoutManager.cpp:20-33), which
// collapses every `<Text>` to nothing unless an explicit size is set. Android
// and Apple each ship a real one, so this is ours, measured with Qt's text
// engine.
//
// The declaration is React Native's own header, unchanged: only the definitions
// are replaced, the same substitution Android and Apple perform.

#include <react/renderer/textlayoutmanager/TextLayoutManager.h>

#include <QFont>
#include <QFontMetricsF>
#include <QList>
#include <QPointF>
#include <QTextLayout>
#include <QTextOption>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace facebook::react {
namespace {

/// Builds a QFont from a text fragment's attributes. Only the properties Qt can
/// honour without a font database of its own are read; anything else is left at
/// Qt's default rather than approximated.
QFont fontForFragment(const TextAttributes& attributes) {
  QFont font;
  if (!attributes.fontFamily.empty()) {
    font.setFamily(QString::fromStdString(attributes.fontFamily));
  }
  if (!std::isnan(attributes.fontSize) && attributes.fontSize > 0) {
    font.setPixelSize(static_cast<int>(attributes.fontSize));
  }
  if (attributes.fontWeight.has_value()) {
    // React Native's weights are the CSS 100..900 scale; Qt's QFont::Weight is
    // the same scale in Qt 6, so this is a direct mapping rather than a guess.
    font.setWeight(static_cast<QFont::Weight>(
        static_cast<int>(*attributes.fontWeight)));
  }
  if (attributes.fontStyle.has_value() &&
      *attributes.fontStyle == FontStyle::Italic) {
    font.setItalic(true);
  }
  return font;
}

}  // namespace

TextLayoutManager::TextLayoutManager(
    const std::shared_ptr<const ContextContainer>& contextContainer)
    : contextContainer_(contextContainer),
      textMeasureCache_(kSimpleThreadSafeCacheSizeCap) {}

TextMeasurement TextLayoutManager::measure(
    const AttributedStringBox& attributedStringBox,
    const ParagraphAttributes& paragraphAttributes,
    const TextLayoutContext& /*layoutContext*/,
    const LayoutConstraints& layoutConstraints) const {
  const auto& attributedString = attributedStringBox.getValue();

  // One string with a format range per fragment, so a paragraph with mixed
  // fonts measures correctly rather than as if it were uniform.
  QString text;
  QList<QTextLayout::FormatRange> formats;
  TextMeasurement::Attachments attachments;

  for (const auto& fragment : attributedString.getFragments()) {
    if (fragment.isAttachment()) {
      // Inline views are laid out by Yoga, not measured here. A zero frame is
      // what React Native's generic manager reports and is honest: this
      // platform has no attachment support.
      attachments.push_back(
          TextMeasurement::Attachment{
              .frame = {.origin = {.x = 0, .y = 0}, .size = {.width = 0, .height = 0}},
              .isClipped = false});
      continue;
    }

    const QString piece = QString::fromStdString(fragment.string);
    QTextLayout::FormatRange range;
    range.start = static_cast<int>(text.length());
    range.length = static_cast<int>(piece.length());
    range.format.setFont(fontForFragment(fragment.textAttributes));
    formats.append(range);
    text += piece;
  }

  if (text.isEmpty()) {
    return TextMeasurement{
        .size =
            {.width = layoutConstraints.minimumSize.width,
             .height = layoutConstraints.minimumSize.height},
        .attachments = attachments};
  }

  QTextLayout layout(text);
  layout.setFormats(formats);

  QTextOption option;
  option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  layout.setTextOption(option);

  // React Native passes infinity for an unconstrained axis, which QTextLine
  // cannot use as a line width.
  const qreal availableWidth =
      std::isfinite(layoutConstraints.maximumSize.width)
      ? static_cast<qreal>(layoutConstraints.maximumSize.width)
      : std::numeric_limits<qreal>::max();

  const int lineLimit = paragraphAttributes.maximumNumberOfLines > 0
      ? paragraphAttributes.maximumNumberOfLines
      : std::numeric_limits<int>::max();

  qreal height = 0;
  qreal width = 0;
  int lines = 0;

  layout.beginLayout();
  while (lines < lineLimit) {
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
      break;
    }
    line.setLineWidth(availableWidth);
    line.setPosition(QPointF(0, height));
    height += line.height();
    width = std::max(width, line.naturalTextWidth());
    ++lines;
  }
  layout.endLayout();

  return TextMeasurement{
      .size =
          {.width = std::clamp(
               static_cast<Float>(width),
               layoutConstraints.minimumSize.width,
               layoutConstraints.maximumSize.width),
           .height = std::clamp(
               static_cast<Float>(height),
               layoutConstraints.minimumSize.height,
               layoutConstraints.maximumSize.height)},
      .attachments = attachments};
}

}  // namespace facebook::react
