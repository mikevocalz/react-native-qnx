// Behavior evidence for the Qt host only. These tests compare Fabric text
// measurement with the actual mounted Qt Quick Text's laid-out content.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/views/QtTextInputQml.h"

#include <react/renderer/components/iostextinput/TextInputProps.h>
#include <react/renderer/components/text/ParagraphProps.h>
#include <react/renderer/components/text/ParagraphState.h>
#include <react/renderer/core/ConcreteState.h>
#include <react/renderer/textlayoutmanager/TextLayoutManager.h>
#include <react/utils/ContextContainer.h>

#include <QColor>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using namespace facebook::react;

void expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void near(qreal actual, qreal expected, const char* message) {
  if (!std::isfinite(actual) || std::abs(actual - expected) > 0.1) {
    throw std::runtime_error(std::string(message) + ": " +
        std::to_string(actual) + " != " + std::to_string(expected));
  }
}

AttributedString paragraph(const TextAttributes& attributes, bool mixed = false) {
  AttributedString result;
  result.setBaseTextAttributes(attributes);
  result.appendFragment({.string = "One two three four five six ",
      .textAttributes = attributes, .parentShadowView = {}});
  if (mixed) {
    auto second = attributes;
    second.fontSize = attributes.fontSize + 3.0F;
    second.fontWeight = FontWeight::Bold;
    result.appendFragment({.string = "seven eight nine & <ten>",
        .textAttributes = second, .parentShadowView = {}});
  }
  return result;
}

struct InputAppearance {
  Size size = {.width = 280, .height = 80};
  EdgeInsets contentInsets = {0};
  Float borderWidth = 0;
  CascadedBorderRadii borderRadii;
  SharedColor backgroundColor;
  SharedColor borderColor;
  bool multiline = false;
};

class Fixture {
 public:
  Fixture()
      : factory_(engine_), layer_(factory_, *window_.contentItem(), 1),
        adapter_(layer_), manager_(std::make_shared<const ContextContainer>()) {
    factory_.registerType(QStringLiteral("Text"), QByteArray("import QtQuick\nText {}"));
    adapter_.mapComponent(QStringLiteral("Paragraph"), QStringLiteral("Text"));
    factory_.registerType(QStringLiteral("TextInput"), QByteArray(rnqnx::views::textInputQml()));
    adapter_.mapComponent(QStringLiteral("TextInput"), QStringLiteral("TextInput"));
  }

  TextMeasurement measure(const AttributedString& text, float width, float dpr = 1.0F) {
    LayoutConstraints constraints;
    constraints.minimumSize = {.width = width, .height = 0};
    constraints.maximumSize = {.width = width, .height = std::numeric_limits<Float>::infinity()};
    return manager_.measure(AttributedStringBox(text), {}, {.pointScaleFactor = dpr}, constraints);
  }

  QQuickItem& mount(const AttributedString& text, float width, int lineLimit = 0) {
    auto props = std::make_shared<ParagraphProps>();
    props->paragraphAttributes.maximumNumberOfLines = lineLimit;
    auto data = std::make_shared<ParagraphState>();
    data->attributedString = text;
    data->paragraphAttributes = props->paragraphAttributes;
    ShadowView next;
    next.tag = 2;
    next.componentName = "Paragraph";
    next.props = props;
    next.state = std::make_shared<const ConcreteState<ParagraphState>>(
        data, ShadowNodeFamily::Weak{});
    next.layoutMetrics.frame.size = measure(text, width).size;
    const auto failures = mounted_
        ? adapter_.apply({ShadowViewMutation::UpdateMutation(previous_, next, 1)})
        : adapter_.apply({ShadowViewMutation::CreateMutation(next),
                          ShadowViewMutation::InsertMutation(1, next, 0)});
    expect(failures.empty(), "Fabric text mutation failed");
    mounted_ = true;
    previous_ = next;
    auto* item = layer_.itemForTag(2);
    expect(item != nullptr, "Text was not mounted");
    expect(QMetaObject::invokeMethod(item, "forceLayout"), "Qt Text forceLayout unavailable");
    return *item;
  }

  void compare(const AttributedString& text, float width) {
    const auto measured = measure(text, width);
    auto& item = mount(text, width);
    near(item.property("contentHeight").toDouble(), measured.size.height,
        "Measured and painted text heights differ");
    expect(item.property("contentWidth").toDouble() <= width + 0.1,
        "Text overflowed the logical content width");
  }

  QQuickItem& mountInput(const TextAttributes& attributes, float scale,
      const std::string& text = {}, const InputAppearance& appearance = {}) {
    auto props = std::make_shared<TextInputProps>();
    props->textAttributes = attributes;
    props->text = text;
    props->placeholder = "Empty field still scales";
    props->backgroundColor = appearance.backgroundColor;
    props->borderColors.all = appearance.borderColor;
    props->borderRadii = appearance.borderRadii;
    props->yogaStyle.setBorder(facebook::yoga::Edge::All,
        facebook::yoga::StyleLength::points(appearance.borderWidth));
    props->multiline = appearance.multiline;
    ShadowView next;
    next.tag = 3;
    next.componentName = "TextInput";
    next.props = props;
    // Empty TextInputs may have no attributed-state fragment from which to
    // recover scale. Exercise the RN LayoutMetrics contract used in production.
    next.layoutMetrics.fontSizeMultiplier = scale;
    next.layoutMetrics.frame.size = appearance.size;
    next.layoutMetrics.contentInsets = appearance.contentInsets;
    next.layoutMetrics.borderWidth = EdgeInsets{appearance.borderWidth};
    const auto failures = inputMounted_
        ? adapter_.apply({ShadowViewMutation::UpdateMutation(previousInput_, next, 1)})
        : adapter_.apply({ShadowViewMutation::CreateMutation(next),
                          ShadowViewMutation::InsertMutation(1, next, mounted_ ? 1 : 0)});
    expect(failures.empty(), "Fabric input mutation failed");
    inputMounted_ = true;
    previousInput_ = next;
    auto* item = layer_.itemForTag(3);
    expect(item != nullptr, "TextInput was not mounted");
    return *item;
  }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
  rnqnx::views::QtQuickItemFactory factory_;
  rnqnx::fabric::QtQuickMountingLayer layer_;
  rnqnx::fabric::ShadowViewMutationAdapter adapter_;
  TextLayoutManager manager_;
  ShadowView previous_;
  ShadowView previousInput_;
  bool mounted_ = false;
  bool inputMounted_ = false;
};

void scaledFontsAndLineHeights() {
  Fixture fixture;
  auto attributes = TextAttributes::defaultTextAttributes();
  attributes.fontSize = 15.5F;
  attributes.fontSizeMultiplier = 1.25F;
  attributes.lineHeight = 22.25F;
  attributes.fontWeight = FontWeight::Bold;
  attributes.fontStyle = FontStyle::Italic;
  auto text = paragraph(attributes);
  fixture.compare(text, 160);
  auto& item = fixture.mount(text, 160);
  auto font = item.property("font").value<QFont>();
  expect(font.pixelSize() == 19, "Fractional font must round after applying font scale");
  expect(font.weight() == QFont::Bold && font.italic(), "Plain text lost weight or italic");
  expect(QFontInfo(font).family() == QStringLiteral("Roboto"),
      "Default font did not use the deployed sans resource");
  near(item.property("lineHeight").toDouble(), 27.8125, "Scaled line height was rounded or ignored");
  near(fixture.measure(text, 160, 1).size.height, fixture.measure(text, 160, 2).size.height,
      "DPR was applied a second time to logical text measurement");

  attributes.maxFontSizeMultiplier = 1.1F;
  text = paragraph(attributes);
  fixture.compare(text, 160);
  expect(fixture.mount(text, 160).property("font").value<QFont>().pixelSize() == 17,
      "maxFontSizeMultiplier did not cap the font");

  attributes.allowFontScaling = false;
  text = paragraph(attributes);
  fixture.compare(text, 160);
  auto& unscaled = fixture.mount(text, 160);
  expect(unscaled.property("font").value<QFont>().pixelSize() == 16,
      "allowFontScaling=false did not disable scaling");
  near(unscaled.property("lineHeight").toDouble(), 22.25, "Disabled scaling changed line height");
}

void mixedTextAndFallbacks() {
  Fixture fixture;
  auto attributes = TextAttributes::defaultTextAttributes();
  attributes.fontSize = 15.5F;
  attributes.fontSizeMultiplier = 1.5F;
  attributes.lineHeight = 30.5F;
  fixture.compare(paragraph(attributes, true), 185);
  fixture.compare(paragraph(attributes, true), 310);
  attributes.fontFamily = "RNQNX Missing Test Family";
  fixture.compare(paragraph(attributes), 185);
  expect(QFontInfo(fixture.mount(paragraph(attributes), 185).property("font").value<QFont>()).family() ==
          QStringLiteral("Roboto"),
      "Missing explicit font did not use the deployed fallback");
  fixture.compare(paragraph(attributes, true), 185);
}

void resetStylesAndNewlines() {
  Fixture fixture;
  auto attributes = TextAttributes::defaultTextAttributes();
  attributes.fontWeight = FontWeight::Bold;
  attributes.fontStyle = FontStyle::Italic;
  attributes.lineHeight = 32;
  fixture.mount(paragraph(attributes), 140, 1);
  attributes = TextAttributes::defaultTextAttributes();
  auto text = paragraph(attributes);
  fixture.compare(text, 140);
  auto& item = fixture.mount(text, 140);
  const auto font = item.property("font").value<QFont>();
  expect(font.pixelSize() == 14 && font.weight() == QFont::Normal && !font.italic(),
      "Removing text styles left stale font properties");
  expect(item.property("lineHeightMode").toInt() == 0, "Natural line height was not restored");
  expect(item.property("maximumLineCount").toInt() == (std::numeric_limits<int>::max)(),
      "Removing numberOfLines left the previous limit");
  AttributedString lines;
  lines.setBaseTextAttributes(attributes);
  lines.appendFragment({.string = "First\nSecond\nThird",
      .textAttributes = attributes, .parentShadowView = {}});
  fixture.compare(lines, 250);
  AttributedString empty;
  empty.setBaseTextAttributes(attributes);
  expect(fixture.mount(empty, 140).property("text").toString().isEmpty(),
      "Clearing a paragraph left old text visible");
}

void inputUsesLayoutFontScale() {
  Fixture fixture;
  TextAttributes attributes;
  attributes.fontSize = 15.5F;
  auto& input = fixture.mountInput(attributes, 1.5F);
  auto* editor = input.findChild<QQuickItem*>(QStringLiteral("qnx-text-input"));
  expect(editor != nullptr, "Production TextInput editor is unavailable");
  const QFont scaled = editor->property("font").value<QFont>();
  expect(scaled.pixelSize() == 23, "Empty input ignored LayoutMetrics font scale");
  expect(QFontInfo(scaled).family() == QStringLiteral("Roboto"),
      "TextInput default did not use the deployed sans resource");
  for (auto* child : input.childItems()) {
    if (child->property("font").isValid()) {
      expect(child->property("font").value<QFont>().pixelSize() == 23,
          "Input editor and placeholder font sizes diverged");
    }
  }
  attributes.maxFontSizeMultiplier = 1.1F;
  fixture.mountInput(attributes, 2.0F, "Retained value");
  expect(editor->property("font").value<QFont>().pixelSize() == 17,
      "TextInput did not cap font scale");
  attributes.allowFontScaling = false;
  fixture.mountInput(attributes, 2.0F, "Retained value");
  expect(editor->property("font").value<QFont>().pixelSize() == 16,
      "TextInput ignored allowFontScaling=false");
  expect(editor->property("text").toString() == QStringLiteral("Retained value"),
      "A font-scale update cleared editable text");
}

void inputSurfaceUsesContentInsets() {
  Fixture fixture;
  TextAttributes attributes;
  attributes.fontSize = 15.5F;
  InputAppearance appearance;
  appearance.contentInsets = {14.5F, 10.25F, 8.5F, 6.75F};
  appearance.backgroundColor = colorFromRGBA(16, 32, 48, 255);
  appearance.borderColor = colorFromRGBA(78, 154, 255, 255);
  appearance.borderWidth = 2;
  appearance.borderRadii.all = ValueUnit{10.5F, UnitType::Point};
  auto& input = fixture.mountInput(attributes, 1.25F, {}, appearance);
  auto* background = input.findChild<QQuickItem*>(QStringLiteral("qnx-text-input-background"));
  auto* editor = input.findChild<QQuickItem*>(QStringLiteral("qnx-text-input"));
  expect(background != nullptr && editor != nullptr, "Production input field surface is unavailable");
  expect(background->property("color").value<QColor>() == QColor(16, 32, 48),
      "TextInput ignored its View backgroundColor");
  auto* border = background->property("border").value<QObject*>();
  expect(border != nullptr, "TextInput background has no native border");
  near(border->property("width").toDouble(), 2, "TextInput ignored borderWidth");
  expect(border->property("color").value<QColor>() == QColor(78, 154, 255),
      "TextInput ignored borderColor");
  near(background->property("radius").toDouble(), 10.5, "TextInput ignored borderRadius");

  const auto expectBounds = [&input, background](const InputAppearance& expected, int pixels) {
    near(input.width(), expected.size.width, "Insets changed the outer Fabric width");
    near(input.height(), expected.size.height, "Insets changed the outer Fabric height");
    near(background->x(), 0, "Background was inset with text");
    near(background->y(), 0, "Background was inset with text");
    near(background->width(), expected.size.width, "Background did not fill the Fabric width");
    near(background->height(), expected.size.height, "Background did not fill the Fabric height");
    for (const auto* name : {"qnx-text-input", "qnx-text-edit", "qnx-text-input-placeholder"}) {
      auto* child = input.findChild<QQuickItem*>(QString::fromLatin1(name));
      expect(child != nullptr, "TextInput content child is unavailable");
      near(child->x(), expected.contentInsets.left, "TextInput content lost its resolved left inset");
      near(child->y(), expected.contentInsets.top, "TextInput content lost its resolved top inset");
      near(child->width(), expected.size.width - expected.contentInsets.left - expected.contentInsets.right,
          "TextInput content width applied padding or border twice");
      near(child->height(), expected.size.height - expected.contentInsets.top - expected.contentInsets.bottom,
          "TextInput content height applied padding or border twice");
      expect(child->clip(), "TextInput content can paint over padding and border");
      expect(child->property("font").value<QFont>().pixelSize() == pixels,
          "Styled input editor and placeholder font scale diverged");
      expect(background->z() < child->z(), "TextInput background covers its text");
    }
  };
  expectBounds(appearance, 19);

  fixture.mountInput(attributes, 1.25F, "Retained value", appearance);
  editor->setProperty("cursorPosition", 5);
  appearance.size = {.width = 176, .height = 72};
  appearance.contentInsets = {8.25F, 12.5F, 16.75F, 9.25F};
  appearance.borderRadii.topLeft = ValueUnit{3, UnitType::Point};
  appearance.borderRadii.topRight = ValueUnit{7, UnitType::Point};
  appearance.borderRadii.bottomRight = ValueUnit{11, UnitType::Point};
  appearance.borderRadii.bottomLeft = ValueUnit{15, UnitType::Point};
  auto& resized = fixture.mountInput(attributes, 1.5F, "Retained value", appearance);
  expect(&resized == &input && input.findChild<QQuickItem*>(QStringLiteral("qnx-text-input")) == editor,
      "Field styling or font scaling remounted the editor");
  expectBounds(appearance, 23);
  near(background->property("topLeftRadius").toDouble(), 3, "Input lost the top-left radius");
  near(background->property("topRightRadius").toDouble(), 7, "Input lost the top-right radius");
  near(background->property("bottomRightRadius").toDouble(), 11, "Input lost the bottom-right radius");
  near(background->property("bottomLeftRadius").toDouble(), 15, "Input lost the bottom-left radius");
  expect(editor->property("text").toString() == QStringLiteral("Retained value") &&
          editor->property("cursorPosition").toInt() == 5,
      "Field bounds or font-scale update changed editable state");

  appearance.multiline = true;
  fixture.mountInput(attributes, 1.5F, "First\nSecond", appearance);
  expectBounds(appearance, 23);
  auto* multiline = input.findChild<QQuickItem*>(QStringLiteral("qnx-text-edit"));
  expect(multiline->property("text").toString() == QStringLiteral("First\nSecond"),
      "Styled multiline editor lost its text");

  appearance = {};
  fixture.mountInput(attributes, 1, "Retained value", appearance);
  expectBounds(appearance, 16);
  expect(background->property("color").value<QColor>().alpha() == 0,
      "Removing backgroundColor left a stale field fill");
  near(border->property("width").toDouble(), 0, "Removing borderWidth left a stale border");
  near(background->property("topLeftRadius").toDouble(), 0, "Removing borderRadius left a stale corner");
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    scaledFontsAndLineHeights();
    mixedTextAndFallbacks();
    resetStylesAndNewlines();
    inputUsesLayoutFontScale();
    inputSurfaceUsesContentInsets();
    std::cout << "RNQNX_QT_TEXT_LAYOUT_OK scaling=1 caps=1 mixed=1 fallback=1 resize=1 reset=1 input=1 surface=1\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
