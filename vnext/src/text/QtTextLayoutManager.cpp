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
#include <QString>

#include <algorithm>
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

  TextMeasurement::Attachments attachments;
  Float width = 0;
  Float height = 0;

  for (const auto& fragment : attributedString.getFragments()) {
    if (fragment.isAttachment()) {
      // Inline views are laid out by Yoga, not measured here. Reporting a zero
      // frame is what the generic implementation does and is honest: this
      // platform has no attachment support yet.
      attachments.push_back(
          TextMeasurement::Attachment{
              .frame = {.origin = {.x = 0, .y = 0}, .size = {.width = 0, .height = 0}},
              .isClipped = false});
      continue;
    }

    const QFontMetricsF metrics(fontForFragment(fragment.textAttributes));
    const QString text = QString::fromStdString(fragment.string);

    // Single-line advance width, summed across fragments because they sit on
    // one line unless wrapped. Wrapping is handled below by clamping.
    width += static_cast<Float>(metrics.horizontalAdvance(text));
    height = std::max(height, static_cast<Float>(metrics.height()));
  }

  // Respect the constraints React Native computed. Clamping rather than
  // re-wrapping means multi-line text reports one line's height, which is
  // recorded as a gap in docs/component-support.md rather than hidden.
  width = std::clamp(
      width, layoutConstraints.minimumSize.width, layoutConstraints.maximumSize.width);
  height = std::clamp(
      height,
      layoutConstraints.minimumSize.height,
      layoutConstraints.maximumSize.height);

  if (paragraphAttributes.maximumNumberOfLines > 0) {
    height = std::min(
        height,
        static_cast<Float>(paragraphAttributes.maximumNumberOfLines) * height);
  }

  return TextMeasurement{
      .size = {.width = width, .height = height}, .attachments = attachments};
}

}  // namespace facebook::react
