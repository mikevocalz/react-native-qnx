// react-native-svg 15.16.0, unmodified, through Metro, Hermes, Fabric, the Qt
// mounting layer and the touch dispatcher. Pixels come from the real window.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <RnsvgForeignObjectItem.h>
#include <RnsvgItems.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

#include <fstream>
#include <functional>
#include <iostream>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

rnqnx::views::RnsvgRootItem* findRoot(QQuickItem* parent) {
  if (auto* item = qobject_cast<rnqnx::views::RnsvgRootItem*>(parent)) return item;
  for (auto* child : parent->childItems()) {
    if (auto* item = findRoot(child)) return item;
  }
  return nullptr;
}

void collectRoots(QQuickItem* parent, std::vector<rnqnx::views::RnsvgRootItem*>& out) {
  if (auto* item = qobject_cast<rnqnx::views::RnsvgRootItem*>(parent)) out.push_back(item);
  for (auto* child : parent->childItems()) collectRoots(child, out);
}

std::vector<rnqnx::views::RnsvgRootItem*> allRoots(QQuickItem* parent) {
  std::vector<rnqnx::views::RnsvgRootItem*> roots;
  collectRoots(parent, roots);
  return roots;
}

// The first element under `parent`, in tree order, of this kind that `accept` takes.
const rnqnx::views::RnsvgElementItem* findElement(
    const QQuickItem* parent, rnqnx::views::rnsvg::Kind kind,
    const std::function<bool(const rnqnx::views::rnsvg::ElementProps&)>& accept = {}) {
  if (const auto* e = qobject_cast<const rnqnx::views::RnsvgElementItem*>(parent)) {
    if (e->kind() == kind && (!accept || accept(e->props()))) return e;
  }
  for (const auto* child : parent->childItems()) {
    if (const auto* found = findElement(child, kind, accept)) return found;
  }
  return nullptr;
}

bool closeTo(double a, double b) { return std::abs(a - b) < 1e-3; }

int countElements(const QQuickItem* parent) {
  int n = qobject_cast<const rnqnx::views::RnsvgElementItem*>(parent) ? 1 : 0;
  for (auto* child : parent->childItems()) n += countElements(child);
  return n;
}

std::string g_diagnostics;  // filled by the test so a timeout explains itself

std::string dumpTree(const QQuickItem* item, int depth = 0) {
  std::string out;
  if (const auto* e = qobject_cast<const rnqnx::views::RnsvgElementItem*>(item)) {
    const auto& p = e->props();
    out += std::string(depth * 2, ' ') + "kind=" + std::to_string(int(p.kind)) +
           " fill=" + std::to_string(int(p.fill.type)) + ":" + p.fill.color.name(QColor::HexArgb).toStdString() +
           " name=" + p.name.toStdString() + " w=" + std::to_string(p.width.value) +
           " propList=" + std::to_string(p.propList.size()) + "\n";
  } else if (const auto* r = qobject_cast<const rnqnx::views::RnsvgRootItem*>(item)) {
    out += std::string(depth * 2, ' ') + "root " + std::to_string(r->width()) + "x" + std::to_string(r->height()) +
           " paints=" + std::to_string(r->paintCount()) + "\n";
  }
  for (const auto* child : item->childItems()) out += dumpTree(child, depth + 1);
  return out;
}

void await(rnqnx::runtime::QtReactHost& host, const std::function<bool()>& ready,
           const char* what = "update") {
  QElapsedTimer timer;
  timer.start();
  do {
    host.runtime().drainMicrotasks();
    host.pumpEvents();
    if (ready()) return;
  } while (timer.elapsed() < 5000);
  std::string message = std::string("Timed out waiting for: ") + what + "\n" + g_diagnostics;
  for (const auto& error : host.javaScriptErrors()) message += "\n  JS error: " + error.message + "\n" + error.stack;
  for (const auto& failure : host.delegate().lastFailures()) message += "\n  mount failure: " + failure.detail.toStdString();
  throw std::runtime_error(message);
}

void evaluate(rnqnx::runtime::QtReactHost& host, const std::string& script) {
  if (!host.evaluate(script, "qt-rnsvg-fabric-test.js")) {
    throw std::runtime_error(host.lastError().toStdString());
  }
}

double number(rnqnx::runtime::QtReactHost& host, const char* name) {
  auto& runtime = host.runtime();
  return runtime.global().getProperty(runtime, name).asNumber();
}

bool near(const QColor& a, const QColor& b, int tolerance = 6) {
  return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance &&
         std::abs(a.blue() - b.blue()) <= tolerance;
}

bool pixelIs(QQuickWindow& window, int x, int y, const QColor& color, int tolerance = 6) {
  const auto image = window.grabWindow();
  return !image.isNull() && near(image.pixelColor(x, y), color, tolerance);
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  try {
    QQuickWindow window;
    window.setColor(Qt::white);
    window.resize(220, 160);
    QQmlEngine engine;
    rnqnx::views::QtQuickItemFactory factory(engine);
    factory.registerType(QStringLiteral("View"),
        "import QtQuick\nItem { property color backgroundColor: 'transparent' }\n");
    rnqnx::fabric::QtQuickMountingLayer layer(factory, *window.contentItem(), 11);
    rnqnx::fabric::ShadowViewMutationAdapter adapter(layer);
    rnqnx::fabric::QtTouchDispatcher dispatcher(layer, *window.contentItem());
    adapter.setTouchDispatcher(&dispatcher);
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    rnqnx::runtime::QtReactHost host(adapter);
    host.delegate().setTouchDispatcher(&dispatcher);
    host.installTurboModuleBinding();
    host.installComponentProvider();
    host.installPlatformConstants();
    host.installTimers();
    for (const char* name : {"RNSVGSvgView", "RNSVGPath", "RNSVGGroup", "RNSVGLinearGradient", "RNSVGMask"}) {
      check(host.hasComponent(name), "react-native-svg descriptor missing from runtime");
    }
    for (const char* name : {"RNSVGPattern", "RNSVGSymbol", "RNSVGMarker", "RNSVGTextPath", "RNSVGForeignObject"}) {
      check(host.hasComponent(name), "react-native-svg descriptor missing from runtime");
    }
    std::ifstream file(RNQNX_RNSVG_BUNDLE_PATH);
    check(file.good(), "Metro react-native-svg fixture bundle is missing");
    const std::string bundle((std::istreambuf_iterator<char>(file)), {});
    evaluate(host, bundle);
    host.startSurface(11, "RnqnxRnsvgFixture", 220.0F, 160.0F);
    window.show();
    QObject input;
    input.installEventFilter(&dispatcher);

    await(host, [&] { return findRoot(window.contentItem()) != nullptr; }, "the <Svg> item to mount");
    check(host.delegate().lastFailures().empty(), "Fabric mount reported a failure");
    check(host.javaScriptErrors().empty(), "React reported a JavaScript error");
    QPointer<rnqnx::views::RnsvgRootItem> root = findRoot(window.contentItem());
    check(root->width() == 200 && root->height() == 150, "Yoga layout did not reach the <Svg> item");
    const int elements = countElements(root);
    check(elements >= 14, "Element tree was not mounted");

    // Gradient, group inheritance, clip, mask, use, path.
    g_diagnostics = dumpTree(window.contentItem());
    await(host, [&] {
      g_diagnostics = dumpTree(window.contentItem()) + "pixel(3,20)=" +
          window.grabWindow().pixelColor(3, 20).name(QColor::HexArgb).toStdString() + " pixel(20,70)=" +
          window.grabWindow().pixelColor(20, 70).name(QColor::HexArgb).toStdString() + "\n";
      return pixelIs(window, 3, 20, QColor(255, 0, 0), 24);  // a few percent into the gradient
    }, "gradient pixels");
    check(pixelIs(window, 97, 20, QColor(0, 0, 255), 24), "gradient end color wrong");
    check(pixelIs(window, 20, 70, QColor(0, 255, 0)), "<G fill> was not inherited");
    check(pixelIs(window, 70, 70, QColor(255, 0, 0)), "element fill did not override <G fill>");
    check(pixelIs(window, 150, 90, QColor(255, 0, 0)), "clipped rect missing inside the clip");
    check(pixelIs(window, 110, 10, QColor(255, 255, 255)), "clipPath did not clip");
    check(pixelIs(window, 50, 110, QColor(0, 0, 255)), "mask hid the visible half");
    check(pixelIs(window, 150, 110, QColor(255, 255, 255)), "mask did not hide the masked half");
    check(pixelIs(window, 110, 130, QColor(255, 128, 0)), "<Use> did not instantiate the definition");
    check(pixelIs(window, 40, 135, QColor(0, 255, 0)), "path fill missing");

    // Prop updates repaint the same retained elements; nothing is remounted.
    evaluate(host, "__rnsvgUpdate({pathFill:'#ffff00', gradientEnd:'#00ff00'});");
    await(host, [&] { return pixelIs(window, 40, 135, QColor(255, 255, 0)); });
    check(pixelIs(window, 97, 20, QColor(0, 255, 0), 24), "gradient stop update did not repaint");
    check(findRoot(window.contentItem()) == root.data(), "<Svg> was remounted by a prop update");
    check(countElements(root) == elements, "Element count changed on a prop update");

    // Conditional children come and go.
    check(pixelIs(window, 170, 30, QColor(128, 0, 128)), "circle missing");
    evaluate(host, "__rnsvgUpdate({showCircle:false});");
    await(host, [&] { return pixelIs(window, 170, 30, QColor(255, 255, 255)); });
    check(countElements(root) == elements - 1, "Removed element was not destroyed");

    // Per-element touch targets: the Path's own onPress fires, and the press on
    // empty space does not.
    const auto mouse = [&](bool down, QPointF point) {
      QMouseEvent event(down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease, point, point,
          Qt::LeftButton, down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(&input, &event);
    };
    const auto settle = [&] {
      QElapsedTimer elapsed;
      elapsed.start();
      while (elapsed.elapsed() < 300) { host.runtime().drainMicrotasks(); host.pumpEvents(); }
    };
    mouse(true, {190, 60});  // outside every responsible element
    settle();
    mouse(false, {190, 60});
    settle();
    check(number(host, "__rnsvgPresses") == 0, "press on empty space reached a path");
    mouse(true, {40, 135});
    settle();
    mouse(false, {40, 135});
    await(host, [&] { return number(host, "__rnsvgPresses") == 1; });
    check(host.javaScriptErrors().empty(), "Touch handling reported a JavaScript error");


    // The elements the first fixture does not use: Pattern, Symbol, Marker,
    // TextPath and ForeignObject, the text, marker and clipRule props, and
    // onSvgLayout. Props are read from the retained elements, which is what
    // the bridge produces; the renderer has its own tests.
    using namespace rnqnx::views::rnsvg;
    evaluate(host, "__rnsvgUpdate({extras:true});");
    await(host, [&] { return allRoots(window.contentItem()).size() == 2; }, "the second <Svg>");
    check(host.delegate().lastFailures().empty(), "Mounting the remaining elements reported a failure");
    check(host.javaScriptErrors().empty(), "Mounting the remaining elements reported a JavaScript error");
    check(root.data() == findRoot(window.contentItem()), "Mounting the second <Svg> remounted the first");
    const QQuickItem* extra = allRoots(window.contentItem()).back();
    check(extra != root.data(), "The second <Svg> was not found");

    const auto* pattern = findElement(extra, Kind::Pattern);
    check(pattern != nullptr, "RNSVGPattern was not mounted");
    {
      const ElementProps& p = pattern->props();
      check(p.name == "pat" && p.x.value == 2 && p.y.value == 3 && p.width.value == 20, "Pattern box props wrong");
      check(p.height.unit == Length::Unit::Percent && p.height.value == 30, "Pattern height percent lost");
      check(p.patternUserSpace && !p.patternContentUserSpace, "Pattern units wrong");
      check(p.patternTransform && closeTo(p.patternTransform->m11(), std::cos(10 * std::acos(-1.0) / 180)),
            "patternTransform did not arrive");
      check(p.viewBox.set && p.viewBox.width == 10 && p.viewBox.align == "xMinYMax" && p.viewBox.meetOrSlice == 1,
            "Pattern viewBox wrong");
    }
    const auto* symbol = findElement(extra, Kind::Symbol);
    check(symbol != nullptr, "RNSVGSymbol was not mounted");
    check(symbol->props().name == "sym" && symbol->props().viewBox.set && symbol->props().viewBox.width == 24 &&
              symbol->props().viewBox.meetOrSlice == 2,
          "Symbol viewBox wrong (preserveAspectRatio none is meetOrSlice 2)");
    const auto* marker = findElement(extra, Kind::Marker);
    check(marker != nullptr, "RNSVGMarker was not mounted");
    {
      const ElementProps& p = marker->props();
      check(p.refX.value == 3 && p.refY.value == 4 && p.markerWidth.value == 6 && p.markerHeight.value == 7,
            "Marker size props wrong");
      check(!p.markerStrokeWidthUnits && p.orient == "auto-start-reverse", "Marker units or orient wrong");
      check(p.viewBox.set && p.viewBox.width == 10, "Marker viewBox wrong");
    }

    const auto* markedPath = findElement(extra, Kind::Path, [](const ElementProps& p) { return !p.markerStart.isEmpty(); });
    check(markedPath != nullptr, "The marked <Path> lost markerStart");
    check(markedPath->props().markerStart == "mk" && markedPath->props().markerMid == "mk" &&
              markedPath->props().markerEnd == "mk",
          "Marker references were not stripped to ids");
    check(markedPath->props().clipRule == 0, "clipRule evenodd did not arrive as 0");
    check(markedPath->props().filter == "absent", "filter did not arrive as its id");
    check(!markedPath->props().vertices.empty(), "Path vertices for markers were not produced");
    check(findElement(extra, Kind::Circle, [](const ElementProps& p) { return p.cx.value == 150; })->props().clipRule == 1,
          "clipRule nonzero did not arrive as 1");
    check(findElement(extra, Kind::Circle, [](const ElementProps& p) { return p.cx.value == 170; })->props().clipRule == -1,
          "An unset clipRule must stay -1 (regenerate the props with scripts/qt-rnsvg-codegen.mjs)");

    const auto* text = findElement(extra, Kind::Text, [](const ElementProps& p) { return !p.text.x.empty(); });
    check(text != nullptr, "RNSVGText was not mounted");
    {
      const ElementProps& p = text->props();
      check(p.text.x.size() == 2 && p.text.x[1].value == 20 && p.text.dx.size() == 1, "Per-character x/dx lists lost");
      check(p.text.rotate.size() == 3 && p.text.rotate[2] == 15, "rotate list lost");
      check(p.text.inlineSize.value == 80 && p.text.baselineShiftKeyword == "sub" &&
                p.text.alignmentBaseline == "middle",
            "inlineSize, baselineShift or alignmentBaseline wrong");
      check(p.font.size && *p.font.size == 14 && p.font.weight && *p.font.weight == 700 && p.font.anchor == "middle",
            "Font size, weight or anchor wrong");
      check(p.font.wordSpacing && *p.font.wordSpacing == 2 && p.font.kerning && *p.font.kerning == 1 &&
                p.font.letterSpacing && *p.font.letterSpacing == 0.5,
            "wordSpacing, kerning or letterSpacing wrong");
      check(p.font.decoration == "underline" && p.font.variantLigatures == "none" && p.font.fontStretch == "condensed" &&
                p.font.fontFeatureSettings == "\"liga\" 0",
            "Font decoration, ligatures, stretch or feature settings wrong");
    }
    const auto* span = findElement(extra, Kind::TSpan, [](const ElementProps& p) { return !p.text.x.empty(); });
    check(span != nullptr, "RNSVGTSpan with its own x was not mounted");
    check(span->props().text.dy.size() == 1 && span->props().text.rotate.size() == 1 &&
              span->props().text.rotate[0] == 45 && span->props().text.baselineShift.value == -4 &&
              span->props().text.alignmentBaseline == "middle",
          "TSpan dy, rotate or verticalAlign (shift and baseline) wrong");
    const auto* textPath = findElement(extra, Kind::TextPath);
    check(textPath != nullptr, "RNSVGTextPath was not mounted");
    {
      const TextPathProps& tp = textPath->props().textPath;
      check(tp.href == "curve" && tp.startOffset.value == 10 && tp.method == "stretch" && tp.spacing == "exact" &&
                tp.midLine == "sharp" && tp.side == "right",
            "TextPath props wrong");
    }

    // ForeignObject: its React Native child is a live item under the
    // ForeignObject's item, and the scene has placed the box.
    const auto* foElement = findElement(extra, Kind::ForeignObject);
    check(foElement != nullptr, "RNSVGForeignObject was not mounted");
    const auto* fo = qobject_cast<const rnqnx::views::RnsvgForeignObjectItem*>(foElement);
    check(fo != nullptr, "RNSVGForeignObject must mount an RnsvgForeignObjectItem (createItemForComponent row)");
    check(foElement->props().x.value == 100 && foElement->props().y.value == 100 && foElement->props().width.value == 80 &&
              foElement->props().height.value == 40,
          "ForeignObject box props wrong");
    int liveChildren = 0;
    for (const auto* child : fo->childItems()) {
      if (!qobject_cast<const rnqnx::views::RnsvgElementItem*>(child)) ++liveChildren;
    }
    check(liveChildren >= 1, "The React Native child was not mounted under the ForeignObject");
    await(host, [&] { return fo->isPlaced(); }, "the scene to place the ForeignObject");
    check(fo->boxSize() == QSizeF(80, 40), "ForeignObject box size was not applied");
    const QPointF boxOrigin = fo->mapToItem(extra, QPointF(0, 0));
    check(closeTo(boxOrigin.x(), 100) && closeTo(boxOrigin.y(), 100), "ForeignObject was not placed at its x and y");
    mouse(true, {115, 110});  // on the live child
    settle();
    mouse(false, {115, 110});
    await(host, [&] { return number(host, "__rnsvgForeignTouches") == 1; }, "a press on the ForeignObject's child");

    // onSvgLayout: once the box is laid out, then again only when it moves.
    await(host, [&] { return number(host, "__rnsvgLayoutEvents") >= 1; }, "onSvgLayout");
    settle();
    check(number(host, "__rnsvgLayoutX") == 10 && number(host, "__rnsvgLayoutY") == 60 &&
              number(host, "__rnsvgLayoutW") == 30 && number(host, "__rnsvgLayoutH") == 20,
          "onSvgLayout rect wrong");
    const double layoutEvents = number(host, "__rnsvgLayoutEvents");
    evaluate(host, "__rnsvgUpdate({rectFill:'#00ffff'});");
    settle();
    check(number(host, "__rnsvgLayoutEvents") == layoutEvents, "onSvgLayout fired although the box did not move");
    evaluate(host, "__rnsvgUpdate({rectX:25});");
    await(host, [&] { return number(host, "__rnsvgLayoutEvents") == layoutEvents + 1; }, "onSvgLayout after a move");
    check(number(host, "__rnsvgLayoutX") == 25, "onSvgLayout did not report the new x");

    evaluate(host, "__rnsvgUpdate({extras:false});");
    await(host, [&] { return allRoots(window.contentItem()).size() == 1; }, "the second <Svg> to unmount");
    check(root.data() == findRoot(window.contentItem()), "Unmounting the second <Svg> disturbed the first");
    check(countElements(root) == elements - 1, "Element count of the first <Svg> changed");
    check(host.javaScriptErrors().empty(), "The remaining elements reported a JavaScript error");

    // Resizing the Yoga box re-lays out the same retained tree.
    evaluate(host, "__rnsvgUpdate({width:100});");
    await(host, [&] { return root->width() == 100; });
    check(root.data() == findRoot(window.contentItem()), "resize remounted <Svg>");

    host.stopSurface(11);
    host.pumpEvents();
    check(host.javaScriptErrors().empty(), "react-native-svg lifecycle reported JavaScript errors");
    std::cout << "RNQNX_RNSVG_FABRIC_OK metro hermes fabric gradient clip mask use update remove touch resize extras\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
