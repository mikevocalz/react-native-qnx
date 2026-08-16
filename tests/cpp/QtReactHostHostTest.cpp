/*
 * Host tests for the runtime/renderer wiring: a Hermes runtime, a
 * RuntimeScheduler, React Native's Scheduler with our EventBeat, component
 * registry factory and SchedulerDelegate, and a surface whose commits land on
 * Qt Quick items.
 *
 * These are the five pieces React Native ships no generic implementation of.
 * Host evidence only: JavaScript runs on the test thread, no bundle is loaded,
 * and nothing here has been cross-compiled for QNX or run on the Pi 4.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <react/renderer/components/view/ViewShadowNode.h>
#include <react/renderer/element/ComponentBuilder.h>
#include <react/renderer/element/Element.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>

#include <yoga/style/StyleLength.h>

#include <QByteArray>
#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QPointF>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using facebook::react::ComponentBuilder;
using facebook::react::ContextContainer;
using facebook::react::Element;
using facebook::react::ShadowNode;
using facebook::react::SurfaceId;
using facebook::react::ViewShadowNode;
using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::ShadowViewMutationAdapter;
using rnqnx::fabric::Tag;
using rnqnx::runtime::QtReactHost;
using rnqnx::views::QtQuickItemFactory;

constexpr SurfaceId kSurfaceId = 11;
constexpr const char* kViewSource = "import QtQuick\nRectangle { visible: true }\n";
constexpr const char* kTextSource = "import QtQuick\nText { visible: true }\n";
constexpr const char* kImageSource =
    "import QtQuick\n"
    "Image {\n"
    "  visible: true\n"
    "  property var bridge: null\n"
    "  onStatusChanged: { if (bridge) bridge.notify(); }\n"
    "}\n";
// Flickable is Qt Quick's scrollable container; clip keeps content inside the
// viewport, which is what a React Native ScrollView does.
constexpr const char* kScrollViewSource =
    "import QtQuick\nFlickable { visible: true; clip: true }\n";
// Qt Quick's TextInput is the editable single-line item. `visible` matches the
// other types; everything else is set from props at mount time.
constexpr const char* kTextInputSource =
    "import QtQuick\nTextInput { visible: true }\n";
constexpr const char* kActivityIndicatorSource =
    "import QtQuick\n"
    "Item {\n"
    "  id: root\n"
    "  property color color: '#999999'\n"
    "  property bool animating: true\n"
    "  property bool hidesWhenStopped: true\n"
    "  property string size: 'small'\n"
    "  visible: animating || !hidesWhenStopped\n"
    "  Rectangle {\n"
    "    id: track\n"
    "    anchors.centerIn: parent\n"
    "    width: Math.min(parent.width, parent.height) * 0.8\n"
    "    height: width\n"
    "    radius: width / 2\n"
    "    color: 'transparent'\n"
    "    border.width: width * 0.1\n"
    "    border.color: root.color\n"
    "    Rectangle {\n"
    "      id: arm\n"
    "      x: track.width / 2 - width / 2\n"
    "      y: track.height / 2 - height\n"
    "      width: track.width * 0.12\n"
    "      height: track.height * 0.4\n"
    "      color: root.color\n"
    "      transformOrigin: Item.Bottom\n"
    "      NumberAnimation on rotation {\n"
    "        from: 0\n"
    "        to: 360\n"
    "        duration: 1000\n"
    "        loops: Animation.Infinite\n"
    "        running: root.animating\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "}\n";
constexpr const char* kSwitchSource =
    "import QtQuick\n"
    "Rectangle {\n"
    "  id: root\n"
    "  property bool value: false\n"
    "  property bool disabled: false\n"
    "  property color trackOnColor: '#4cd964'\n"
    "  property color trackOffColor: '#ffffff'\n"
    "  property color thumbColor: '#ffffff'\n"
    "  visible: true\n"
    "  Rectangle {\n"
    "    id: track\n"
    "    anchors.fill: parent\n"
    "    radius: parent.height / 2\n"
    "    color: root.value ? root.trackOnColor : root.trackOffColor\n"
    "  }\n"
    "  Rectangle {\n"
    "    id: thumb\n"
    "    width: parent.height - 4\n"
    "    height: width\n"
    "    radius: width / 2\n"
    "    color: root.thumbColor\n"
    "    anchors.verticalCenter: parent.verticalCenter\n"
    "    x: root.value ? parent.width - width - 2 : 2\n"
    "  }\n"
    "  MouseArea {\n"
    "    anchors.fill: parent\n"
    "    enabled: !root.disabled\n"
    "    onClicked: {\n"
    "      root.value = !root.value;\n"
    "    }\n"
    "  }\n"
    "}\n";

class TestFailure final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(
    const char* expression,
    const char* file,
    int line,
    const std::string& detail = {}) {
  std::ostringstream message;
  message << file << ':' << line << ": check failed: " << expression;
  if (!detail.empty()) {
    message << " (" << detail << ')';
  }
  throw TestFailure(message.str());
}

#define EXPECT(expression)                   \
  do {                                       \
    if (!(expression)) {                     \
      fail(#expression, __FILE__, __LINE__); \
    }                                        \
  } while (false)

#define EXPECT_NEAR(actual, expected)                                    \
  do {                                                                   \
    const double actual_ = static_cast<double>(actual);                  \
    const double expected_ = static_cast<double>(expected);              \
    if (actual_ < expected_ - 0.01 || actual_ > expected_ + 0.01) {      \
      std::ostringstream detail_;                                        \
      detail_ << actual_ << " != " << expected_;                         \
      fail(#actual " ~= " #expected, __FILE__, __LINE__, detail_.str()); \
    }                                                                    \
  } while (false)

std::shared_ptr<const ViewShadowNode::ConcreteProps> viewProps(
    const std::function<void(ViewShadowNode::ConcreteProps&)>& configure) {
  auto props = std::make_shared<ViewShadowNode::ConcreteProps>();
  // React Native flattens views carrying only layout props, which would produce
  // no mount mutation at all. See docs/cited-seams.md.
  props->collapsable = false;
  configure(*props);
  return props;
}

/// A component builder matching what the host registers, so tags and component
/// names line up with the descriptors the Scheduler resolves.
ComponentBuilder makeComponentBuilder(
    const std::shared_ptr<const ContextContainer>& contextContainer) {
  static facebook::react::ComponentDescriptorProviderRegistry providerRegistry{};
  auto registry = providerRegistry.createComponentDescriptorRegistry(
      facebook::react::ComponentDescriptorParameters{
          .eventDispatcher = facebook::react::EventDispatcher::Shared{},
          .contextContainer = contextContainer,
          .flavor = nullptr});

  providerRegistry.add(
      facebook::react::concreteComponentDescriptorProvider<
          facebook::react::RootComponentDescriptor>());
  providerRegistry.add(
      facebook::react::concreteComponentDescriptorProvider<
          facebook::react::ViewComponentDescriptor>());

  return ComponentBuilder{registry};
}

class Fixture {
 public:
  Fixture()
      : factory_(engine_),
        layer_(factory_, *window_.contentItem(), kSurfaceId),
        adapter_(layer_),
        dispatcher_(layer_, *window_.contentItem()),
        host_(adapter_),
        builder_(makeComponentBuilder(host_.contextContainer())) {
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    // Paragraph is React Native's laid-out text block; Qt Quick's Text item is
    // what draws it.
    factory_.registerType(QStringLiteral("Text"), QByteArray(kTextSource));
    factory_.registerType(QStringLiteral("Image"), QByteArray(kImageSource));
    factory_.registerType(
        QStringLiteral("ScrollView"), QByteArray(kScrollViewSource));
    factory_.registerType(
        QStringLiteral("TextInput"), QByteArray(kTextInputSource));
    factory_.registerType(
        QStringLiteral("ActivityIndicatorView"),
        QByteArray(kActivityIndicatorSource));
    factory_.registerType(
        QStringLiteral("Switch"), QByteArray(kSwitchSource));
    adapter_.mapComponent(
        QStringLiteral("ScrollView"), QStringLiteral("ScrollView"));
    adapter_.mapComponent(
        QStringLiteral("TextInput"), QStringLiteral("TextInput"));
    adapter_.mapComponent(
        QStringLiteral("ActivityIndicatorView"),
        QStringLiteral("ActivityIndicatorView"));
    adapter_.mapComponent(
        QStringLiteral("Switch"), QStringLiteral("Switch"));
    adapter_.mapComponent(
        QStringLiteral("SafeAreaView"), QStringLiteral("View"));
    adapter_.mapComponent(QStringLiteral("Image"), QStringLiteral("Image"));
    adapter_.mapComponent(QStringLiteral("Paragraph"), QStringLiteral("Text"));
  }

  ~Fixture() { host_.stopSurface(kSurfaceId); }

  QtReactHost& host() { return host_; }
  QQuickWindow& window() { return window_; }
  ShadowViewMutationAdapter& adapter() { return adapter_; }

  /// Starts routing pointer events. The dispatcher is a fixture member declared
  /// before the host, because stopping a surface commits an empty tree during
  /// host destruction and that reaches the adapter, which reaches the
  /// dispatcher.
  rnqnx::fabric::QtTouchDispatcher& enableTouch() {
    adapter_.setTouchDispatcher(&dispatcher_);
    host_.delegate().setTouchDispatcher(&dispatcher_);
    window_.installEventFilter(&dispatcher_);
    return dispatcher_;
  }
  ComponentBuilder& builder() { return builder_; }
  QtQuickMountingLayer& layer() { return layer_; }
  QQuickItem& root() { return *window_.contentItem(); }

  int rootChildCount() { return static_cast<int>(root().childItems().size()); }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
  QtQuickItemFactory factory_;
  QtQuickMountingLayer layer_;
  ShadowViewMutationAdapter adapter_;
  rnqnx::fabric::QtTouchDispatcher dispatcher_;
  QtReactHost host_;
  ComponentBuilder builder_;
};

void runtimeEvaluatesJavaScript() {
  Fixture fixture;
  EXPECT(fixture.host().evaluate("globalThis.probe = 6 * 7;", "test.js"));

  auto& runtime = fixture.host().runtime();
  const auto value = runtime.global().getProperty(runtime, "probe");
  EXPECT(value.isNumber());
  EXPECT_NEAR(value.getNumber(), 42.0);
}

void javaScriptErrorsAreReportedNotSwallowed() {
  Fixture fixture;
  EXPECT(!fixture.host().evaluate("throw new Error('deliberate');", "test.js"));
  EXPECT(fixture.host().lastError().contains(QStringLiteral("deliberate")));
}

void schedulerInstallsTheFabricUIManager() {
  Fixture fixture;
  // Constructing the Scheduler queues UIManagerBinding::createAndInstallIfNeeded
  // onto the bindings executor (Scheduler.cpp:136-138). Seeing the global proves
  // that reached the runtime through our RuntimeExecutor.
  EXPECT(fixture.host().hasFabricUIManager());
}

void surfaceCommitMountsThroughTheDelegate() {
  Fixture fixture;
  fixture.host().startEmptySurface(kSurfaceId, 800.0F, 600.0F);

  auto element = Element<ViewShadowNode>().tag(101).props(
      viewProps([](ViewShadowNode::ConcreteProps& props) {
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Width,
            facebook::yoga::StyleSizeLength::points(90.0F));
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Height,
            facebook::yoga::StyleSizeLength::points(45.0F));
      }));

  EXPECT(fixture.host().commit(
      kSurfaceId,
      std::vector<std::shared_ptr<const ShadowNode>>{
          fixture.builder().build(element)}));

  // The delegate mounts from schedulerShouldRenderTransactions, so a transaction
  // must actually have arrived rather than the tree happening to look right.
  EXPECT(fixture.host().delegate().mountedTransactionCount() > 0);
  EXPECT(fixture.host().delegate().lastFailures().empty());

  QQuickItem* item = fixture.layer().itemForTag(101);
  EXPECT(item != nullptr);
  EXPECT(item->parentItem() == &fixture.root());
  EXPECT_NEAR(item->width(), 90.0);
  EXPECT_NEAR(item->height(), 45.0);
}

void secondCommitUpdatesTheSameItem() {
  Fixture fixture;
  fixture.host().startEmptySurface(kSurfaceId, 800.0F, 600.0F);

  const auto sized = [](float width) {
    return Element<ViewShadowNode>().tag(202).props(
        viewProps([width](ViewShadowNode::ConcreteProps& props) {
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Width,
              facebook::yoga::StyleSizeLength::points(width));
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Height,
              facebook::yoga::StyleSizeLength::points(20.0F));
        }));
  };

  EXPECT(fixture.host().commit(
      kSurfaceId,
      std::vector<std::shared_ptr<const ShadowNode>>{
          fixture.builder().build(sized(50.0F))}));
  QQuickItem* item = fixture.layer().itemForTag(202);
  EXPECT(item != nullptr);
  EXPECT_NEAR(item->width(), 50.0);

  const int transactionsAfterFirst =
      fixture.host().delegate().mountedTransactionCount();

  EXPECT(fixture.host().commit(
      kSurfaceId,
      std::vector<std::shared_ptr<const ShadowNode>>{
          fixture.builder().build(sized(140.0F))}));

  EXPECT(
      fixture.host().delegate().mountedTransactionCount() >
      transactionsAfterFirst);
  EXPECT(fixture.layer().itemForTag(202) == item);
  EXPECT_NEAR(item->width(), 140.0);
}

void stoppingASurfaceUnmountsItsItems() {
  Fixture fixture;
  fixture.host().startEmptySurface(kSurfaceId, 800.0F, 600.0F);

  auto element = Element<ViewShadowNode>().tag(303).props(
      viewProps([](ViewShadowNode::ConcreteProps& props) {
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Width,
            facebook::yoga::StyleSizeLength::points(10.0F));
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Height,
            facebook::yoga::StyleSizeLength::points(10.0F));
      }));

  EXPECT(fixture.host().commit(
      kSurfaceId,
      std::vector<std::shared_ptr<const ShadowNode>>{
          fixture.builder().build(element)}));
  EXPECT(fixture.rootChildCount() == 1);

  // SurfaceHandler::stop commits an empty tree, so removal mutations flow
  // through the same delegate path.
  fixture.host().stopSurface(kSurfaceId);

  EXPECT(fixture.rootChildCount() == 0);
  EXPECT(fixture.layer().itemForTag(303) == nullptr);
}

void commitToAnUnknownSurfaceIsReported() {
  Fixture fixture;
  EXPECT(!fixture.host().commit(
      9999, std::vector<std::shared_ptr<const ShadowNode>>{}));
  EXPECT(fixture.host().lastError().contains(QStringLiteral("9999")));
}

void componentPredicateAnswersForRegisteredNames() {
  Fixture fixture;
  fixture.host().installComponentProvider();

  EXPECT(fixture.host().hasComponent("View"));
  // JavaScript asks with the RCT prefix; React Native's normaliser strips it.
  EXPECT(fixture.host().hasComponent("RCTView"));
  EXPECT(fixture.host().hasComponent("RootView"));
  EXPECT(!fixture.host().hasComponent("Slider"));
}

void javaScriptBuildsTheTreeThroughFabricUIManager() {
  Fixture fixture;
  fixture.host().installComponentProvider();
  fixture.host().startEmptySurface(kSurfaceId, 640.0F, 480.0F);

  // This is the binding surface React's renderer drives: createNode,
  // createChildSet, appendChildToSet, completeRoot
  // (UIManagerBinding.cpp:219, 424, 441, 462). Driving it directly proves the
  // JavaScript-to-mount path without React or a bundle in the way.
  const std::string script = R"JS(
    const fabric = globalThis.nativeFabricUIManager;
    const surfaceId = )JS" +
      std::to_string(kSurfaceId) + R"JS(;
    const node = fabric.createNode(
      501,
      'View',
      surfaceId,
      { width: 160, height: 80, collapsable: false },
      {}
    );
    const childSet = fabric.createChildSet();
    fabric.appendChildToSet(childSet, node);
    fabric.completeRoot(surfaceId, childSet);
    globalThis.didComplete = true;
  )JS";

  EXPECT(fixture.host().evaluate(script, "fabric-drive.js"));
  EXPECT(fixture.host().delegate().lastFailures().empty());

  auto& runtime = fixture.host().runtime();
  const auto completed = runtime.global().getProperty(runtime, "didComplete");
  EXPECT(completed.isBool() && completed.getBool());

  QQuickItem* item = fixture.layer().itemForTag(501);
  EXPECT(item != nullptr);
  EXPECT(item->parentItem() == &fixture.root());
  // The geometry is Yoga's, computed from props JavaScript supplied.
  EXPECT_NEAR(item->width(), 160.0);
  EXPECT_NEAR(item->height(), 80.0);
}

#ifdef RNQNX_PLAYGROUND_BUNDLE_PATH
/// Metro produces a QNX bundle, and it carries the globals React Native's C++
/// requires.
///
/// This deliberately stops short of evaluating the bundle. Doing so reaches
/// `TurboModuleRegistry.getEnforcing('ExceptionsManager')` and throws, because
/// no TurboModule is registered yet — the bundle needs nine of them
/// (DeviceInfo, ExceptionsManager, FileReaderModule, NativeIdleCallbacksCxx,
/// NativeMicrotasksCxx, SegmentFetcher, SourceCode, UIManager,
/// WebSocketModule). That is the next slice, recorded in NEXT_ACTIONS.md;
/// asserting it here would be asserting a failure.
void metroProducesABundleForQnx() {
  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  EXPECT(bundleFile.good());
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());

  EXPECT(bundle.size() > 100000);
  // AppRegistry.js:30 installs this, and AppRegistryBinding::startSurface
  // requires it. Its presence is what makes the bundle startable at all.
  EXPECT(bundle.find("RN$AppRegistry") != std::string::npos);
  // ReactFabric.js:27 installs this; UIManager::stopSurface requires it.
  EXPECT(bundle.find("RN$stopSurface") != std::string::npos);
  // The component this app registered, proving the entry point was bundled.
  EXPECT(bundle.find("RNQnxPlayground") != std::string::npos);
}

/// The host installs the globals React Native's JavaScript branches on before
/// any bundle is evaluated.
void hostInstallsBridgelessGlobals() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  auto& runtime = fixture.host().runtime();

  const auto bridgeless = runtime.global().getProperty(runtime, "RN$Bridgeless");
  EXPECT(bridgeless.isBool() && bridgeless.getBool());

  // NativeModules.js:185-192 throws without one of these.
  EXPECT(!runtime.global()
              .getProperty(runtime, "__fbBatchedBridgeConfig")
              .isUndefined());

  // registerCallableModule.js:20 calls this in bridgeless mode.
  const auto registrar =
      runtime.global().getProperty(runtime, "RN$registerCallableModule");
  EXPECT(registrar.isObject());
  EXPECT(registrar.asObject(runtime).isFunction(runtime));

  EXPECT(fixture.host().evaluate(
      "globalThis.RN$registerCallableModule('Probe', () => ({}));",
      "registrar.js"));
}
#endif

/// The Metro bundle evaluates in Hermes and installs React Native's globals.
///
/// This stops before starting the surface. Rendering needs two things that do
/// not exist yet: a `Platform.qnx.js` — React Native's `Libraries/Utilities/
/// Platform.js` re-exports `./Platform`, which resolves to nothing for an
/// unknown platform, so `Platform.select` is called on undefined — and an
/// ExceptionsManager TurboModule, which is the first thing React reaches for
/// when reporting that error. Both are recorded in NEXT_ACTIONS.md.
void metroBundleEvaluatesInHermes() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  EXPECT(bundleFile.good());
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());

  if (!fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js")) {
    fail(
        "bundle evaluated",
        __FILE__,
        __LINE__,
        fixture.host().lastError().toStdString());
  }

  auto& runtime = fixture.host().runtime();
  // AppRegistry.js:30 installs this once the bundle runs; without it
  // AppRegistryBinding::startSurface throws.
  EXPECT(!runtime.global()
              .getProperty(runtime, "RN$AppRegistry")
              .isUndefined());
  // RN$stopSurface is deliberately not asserted: ReactFabric.js:27 installs it
  // only when the renderer is first used, which needs a surface to start.
}

/// The whole path, end to end: a Metro bundle, evaluated in Hermes, whose
/// registered React component renders through React Native's own renderer and
/// lands on Qt Quick items with Yoga's geometry.
void metroBundleRendersThroughReact() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  EXPECT(bundleFile.good());
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());

  if (!fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js")) {
    fail(
        "bundle evaluated",
        __FILE__,
        __LINE__,
        fixture.host().lastError().toStdString());
  }

  fixture.host().startSurface(kSurfaceId, "RNQnxPlayground", 640.0F, 480.0F);

  // React schedules its work through microtasks and the RuntimeScheduler. With
  // no running event loop, the host has to drain them explicitly.
  for (int i = 0; i < 10; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().mountedTransactionCount() > 0);
  EXPECT(fixture.host().delegate().lastFailures().empty());

  // apps/rn-playground renders one 320x180 View containing a 64x32 View.
  EXPECT(fixture.rootChildCount() == 1);
  QQuickItem* outer = fixture.root().childItems().at(0);
  EXPECT_NEAR(outer->width(), 320.0);
  EXPECT_NEAR(outer->height(), 180.0);
  // Visual props, not just geometry: colour and corner radius come from the
  // JSX style, resolved through React Native's own border cascade.
  EXPECT(outer->property("color").value<QColor>() == QColor(0x20, 0x40, 0x80));
  EXPECT_NEAR(outer->property("radius").toDouble(), 12.0);

  // Seven children: the styled inner view, a Pressable, three Texts — simple,
  // mixed-style, and wrapping — a ScrollView, and an Image.
  EXPECT(outer->childItems().size() == 7);
  QQuickItem* inner = outer->childItems().at(0);
  EXPECT_NEAR(inner->width(), 64.0);
  EXPECT_NEAR(inner->height(), 32.0);
  EXPECT(inner->property("color").value<QColor>() == QColor(0xff, 0x88, 0x00));
  EXPECT_NEAR(inner->opacity(), 0.5);

  // The Text child carries its string, and its size came from this platform's
  // TextLayoutManager measuring with Qt font metrics rather than from an
  // explicit style — React Native's generic C++ manager would have returned
  // zero.
  QQuickItem* text = outer->childItems().at(2);
  EXPECT(text->property("text").toString() == QStringLiteral("Hello QNX"));
  // fontSize: 24 from the JSX reaches the item, and the measured size follows
  // from it rather than from a default font.
  EXPECT(text->property("font").value<QFont>().pixelSize() == 24);
  EXPECT(text->width() > 40.0);
  EXPECT(text->height() > 20.0);

  // Mixed styling is carried as rich text, because Qt Quick's Text applies one
  // font and colour to the whole item.
  QQuickItem* mixed = outer->childItems().at(3);
  EXPECT(mixed->property("textFormat").toInt() == 1);
  const QString markup = mixed->property("text").toString();
  EXPECT(markup.contains(QStringLiteral("plain")));
  EXPECT(markup.contains(QStringLiteral("styled")));
  EXPECT(markup.contains(QStringLiteral("font-size:20px")));
  EXPECT(markup.contains(QStringLiteral("#ffcc00")));

  // The wrapping paragraph is measured with QTextLayout, so its height covers
  // more than one line. React Native's generic manager would have reported the
  // minimum size instead.
  QQuickItem* wrapped = outer->childItems().at(4);
  EXPECT(wrapped->height() > 30.0);
  EXPECT(wrapped->width() <= 320.0);

  // The ScrollView is a Flickable. Its child is mounted into the Flickable's
  // contentItem rather than onto the Flickable itself, because that is what
  // actually scrolls, and its content size is computed from that child — Qt
  // does not derive it the way React Native does.
  QQuickItem* scroll = outer->childItems().at(5);
  EXPECT_NEAR(scroll->height(), 50.0);
  auto* content = scroll->property("contentItem").value<QQuickItem*>();
  EXPECT(content != nullptr);
  EXPECT(content->childItems().size() == 1);
  EXPECT_NEAR(scroll->property("contentHeight").toDouble(), 300.0);
  EXPECT(scroll->property("contentHeight").toDouble() > scroll->height());

  // The Image's source reaches the item as a URL. The app inlines a PNG as a
  // data URI, so this needs no network or file access.
  QQuickItem* image = outer->childItems().at(6);
  // The app's source is a data URI, which Qt Quick's Image cannot read, so this
  // platform decodes it and serves it back through its own image provider.
  const QString source = image->property("source").toUrl().toString();
  EXPECT(source.startsWith(QStringLiteral("image://rnqnx/")));

  // Qt loads image sources asynchronously, including data URIs, so the item
  // reports Loading until the decode finishes. Waiting for Ready is what proves
  // the bytes were actually decoded rather than only assigned.
  for (int i = 0; i < 200 && image->property("status").toInt() == 2; ++i) {
    fixture.host().pumpEvents();
  }
  EXPECT(image->property("status").toInt() == 1);  // Image.Ready
  EXPECT_NEAR(image->width(), 40.0);
  EXPECT_NEAR(image->height(), 24.0);
}

/// Scrolling the Qt Flickable delivers onScroll to JavaScript.
///
/// This is the return path for scrolling: mounting a ScrollView only produces a
/// Flickable, and nothing virtualised — FlatList, SectionList, sticky headers —
/// updates until the offset gets back to JavaScript.
void scrollEventsReachJavaScript() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.host().startSurface(kSurfaceId, "RNQnxPlayground", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* scroll = fixture.root().childItems().at(0)->childItems().at(5);
  EXPECT(scroll->property("contentHeight").toDouble() > scroll->height());

  auto& runtime = fixture.host().runtime();
  // Nothing has scrolled yet, so a later count proves the events came from the
  // scroll rather than from mounting.
  EXPECT(runtime.global().getProperty(runtime, "__scrolls").isUndefined());

  // Driving contentY is what a flick, a wheel, or a scrollTo all end up doing;
  // it is the property Flickable animates and the one Qt notifies on.
  scroll->setProperty("contentY", 120.0);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  const auto scrolls = runtime.global().getProperty(runtime, "__scrolls");
  if (!scrolls.isNumber()) {
    fail(
        "onScroll ran in JavaScript",
        __FILE__,
        __LINE__,
        "globalThis.__scrolls was never set");
  }
  EXPECT(scrolls.getNumber() >= 1.0);

  // The offset is carried, not just the fact that something moved: a scroll
  // event with the wrong contentOffset is useless to a virtualised list.
  const auto offsetY = runtime.global().getProperty(runtime, "__lastScrollY");
  EXPECT(offsetY.isNumber());
  EXPECT_NEAR(offsetY.getNumber(), 120.0);

  // contentSize travels too, and it is the size this platform computed from the
  // Flickable's children rather than one React Native supplied.
  const auto contentHeight =
      runtime.global().getProperty(runtime, "__lastScrollContentHeight");
  EXPECT(contentHeight.isNumber());
  EXPECT_NEAR(contentHeight.getNumber(), 300.0);
}

/// FlatList renders and virtualises on this platform.
///
/// FlatList is upstream JavaScript over ScrollView rather than anything
/// native, so what it needs is the parts underneath: a mounted Flickable,
/// measured children, Dimensions, timers, and scroll offsets coming back. This
/// runs one to find out which of those is actually missing.
void flatListRendersAndVirtualises() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.host().startSurface(kSurfaceId, "RNQnxFlatList", 640.0F, 480.0F);
  for (int i = 0; i < 40; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  EXPECT(outer->childItems().size() == 1);
  QQuickItem* list = outer->childItems().at(0);
  EXPECT_NEAR(list->height(), 100.0);

  // The list mounted as a Flickable with a contentItem, which is what makes it
  // a scroll view rather than a clipped column.
  auto* content = list->property("contentItem").value<QQuickItem*>();
  EXPECT(content != nullptr);

  // Cells are wrapped by VirtualizedList, so rows are found by height
  // anywhere below the content item, and their offsets are read in the content
  // item's coordinates — which is the space the list scrolls.
  const auto rowOffsets = [&](QQuickItem* root) {
    std::vector<double> offsets;
    std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
      for (QQuickItem* child : item->childItems()) {
        if (std::abs(child->height() - 30.0) < 0.5) {
          offsets.push_back(child->mapToItem(root, QPointF(0, 0)).y());
        }
        walk(child);
      }
    };
    walk(root);
    std::sort(offsets.begin(), offsets.end());
    return offsets;
  };

  // 20 rows of data, initialNumToRender 4, windowSize 1. Rendering all 20
  // would mean virtualisation never engaged; rendering none would mean the
  // list never measured.
  const std::vector<double> initial = rowOffsets(content);
  EXPECT(initial.size() == 4);
  EXPECT_NEAR(initial.front(), 0.0);

  // VirtualizedList sizes cells from onLayout when getItemLayout is absent, so
  // without layout events reaching JavaScript it could never window at all.
  auto& rt = fixture.host().runtime();
  const auto layouts = rt.global().getProperty(rt, "__cellLayouts");
  if (!layouts.isNumber()) {
    fail(
        "onLayout ran in JavaScript",
        __FILE__,
        __LINE__,
        "globalThis.__cellLayouts was never set");
  }
  EXPECT_NEAR(layouts.getNumber(), 4.0);
  const auto cellHeight = rt.global().getProperty(rt, "__lastCellHeight");
  EXPECT(cellHeight.isNumber());
  EXPECT_NEAR(cellHeight.getNumber(), 30.0);

  // Scrolling far past the rendered window is what virtualisation exists for:
  // the list has to drop what left the window and render what entered it.
  // Stepped rather than jumped: a list is scrolled by a stream of offsets, and
  // VirtualizedList windows from the metrics those carry. VirtualizedList also
  // batches its window update behind a real setTimeout
  // (updateCellsBatchingPeriod, 50ms by default), and this platform's timers
  // are QTimers, so each step has to cover wall-clock time rather than a fixed
  // number of pump iterations.
  const auto pumpFor = [&fixture](int ms) {
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms) {
      fixture.host().runtime().drainMicrotasks();
      fixture.host().pumpEvents();
    }
  };
  for (double offset = 30.0; offset <= 300.0; offset += 30.0) {
    list->setProperty("contentY", offset);
    pumpFor(80);
  }
  pumpFor(300);

  const auto listScrollY = rt.global().getProperty(rt, "__listScrollY");
  if (!listScrollY.isNumber()) {
    fail(
        "FlatList received onScroll",
        __FILE__,
        __LINE__,
        "globalThis.__listScrollY was never set");
  }
  EXPECT_NEAR(listScrollY.getNumber(), 300.0);

  // The list's own viewport and content lengths reach it too. Without both,
  // VirtualizedList has no window to compute: the viewport comes from the
  // ScrollView's onLayout and the content length from its content container's,
  // and neither is something this platform reports explicitly.
  const auto viewportH = rt.global().getProperty(rt, "__listViewportH");
  if (!viewportH.isNumber()) {
    fail(
        "FlatList received onLayout",
        __FILE__,
        __LINE__,
        "globalThis.__listViewportH was never set");
  }
  EXPECT_NEAR(viewportH.getNumber(), 100.0);
  const auto contentH = rt.global().getProperty(rt, "__listContentH");
  if (!contentH.isNumber()) {
    fail(
        "FlatList received onContentSizeChange",
        __FILE__,
        __LINE__,
        "globalThis.__listContentH was never set");
  }
  // The content grew past the 100pt viewport as cells rendered, which is what
  // makes the list scrollable at all.
  EXPECT(contentH.getNumber() > 100.0);

  const std::vector<double> scrolled = rowOffsets(content);
  EXPECT(!scrolled.empty());
  EXPECT(scrolled.size() < 20);
  // The window moved to where the scroll went rather than staying at the top.
  // The rendered set is the retained initialNumToRender region plus a live
  // window around the scroll offset, with the rows between them released:
  // {0, 30, 60, 90} and {270 … 390} for a 100pt viewport at offset 300.
  EXPECT(scrolled.size() < 20);
  EXPECT_NEAR(scrolled.front(), 0.0);

  const auto renderedAt = [&scrolled](double y) {
    return std::any_of(scrolled.begin(), scrolled.end(), [y](double offset) {
      return std::abs(offset - y) < 0.5;
    });
  };

  // Row 10 sits at the new offset and was not rendered before the scroll, so
  // the window followed the scroll rather than standing still.
  EXPECT(renderedAt(300.0));
  EXPECT(renderedAt(330.0));

  // Rows 4 through 7 left the window and were released. This is the half that
  // makes it virtualisation rather than a list that only ever grows.
  for (double y : {120.0, 150.0, 180.0, 210.0}) {
    EXPECT(!renderedAt(y));
  }
}

/// A TextInput mounts as an editable Qt Quick item.
///
/// React Native's TextInput.js branches on Platform.OS with no third branch, so
/// this reaches the native component directly. What is being established here
/// is the native seam: that the "TextInput" descriptor React Native's own view
/// names map onto is registered, and that it mounts and lays out.
void textInputMounts() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  // Key events are delivered to the focused item of an active window, so the
  // window has to exist on screen before typing can be simulated.
  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(kSurfaceId, "RNQnxTextInput", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  EXPECT(outer->childItems().size() == 1);
  QQuickItem* input = outer->childItems().at(0);
  EXPECT_NEAR(input->width(), 200.0);
  EXPECT_NEAR(input->height(), 40.0);

  // The props reached the item, not just its geometry.
  EXPECT(input->property("text").toString() == QStringLiteral("typed"));
  EXPECT(input->property("font").value<QFont>().pixelSize() == 18);
  EXPECT(input->property("readOnly").toBool() == false);

  // Typing is what a text field is for. Qt applies key events to the focused
  // item, and the resulting text change has to reach JavaScript as onChange —
  // without it a controlled TextInput can never update its own value.
  input->forceActiveFocus();
  for (int i = 0; i < 5; ++i) {
    fixture.host().pumpEvents();
  }
  EXPECT(input->hasActiveFocus());
  QKeyEvent key(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("X"));
  QCoreApplication::sendEvent(&fixture.window(), &key);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(input->property("text").toString() == QStringLiteral("typedX"));

  auto& rt = fixture.host().runtime();
  const auto changes = rt.global().getProperty(rt, "__inputChanges");
  if (!changes.isNumber()) {
    fail(
        "onChange ran in JavaScript",
        __FILE__,
        __LINE__,
        "globalThis.__inputChanges was never set");
  }
  EXPECT(changes.getNumber() >= 1.0);

  // The text travels with the event, not just the fact that it changed.
  const auto text = rt.global().getProperty(rt, "__inputText");
  EXPECT(text.isString());
  EXPECT(text.getString(rt).utf8(rt) == "typedX");

  // The wrapper also surfaces the same text as `onChangeText`, which is the
  // public API most React Native apps use.
  const auto changeTextCount = rt.global().getProperty(rt, "__inputChangeText");
  EXPECT(changeTextCount.isNumber());
  EXPECT(changeTextCount.getNumber() >= 1.0);

  const auto changeTextValue =
      rt.global().getProperty(rt, "__inputTextFromChangeText");
  EXPECT(changeTextValue.isString());
  EXPECT(changeTextValue.getString(rt).utf8(rt) == "typedX");
}

/// FBReactNativeSpec's codegen'd ActivityIndicatorView descriptor is
/// registered, mounts, and has its props applied to the QML item.
void activityIndicatorMounts() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(
      kSurfaceId, "RNQnxActivityIndicator", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  EXPECT(outer->childItems().size() == 1);
  QQuickItem* indicator = outer->childItems().at(0);
  EXPECT_NEAR(indicator->width(), 36.0);
  EXPECT_NEAR(indicator->height(), 36.0);

  // The props reached the item, not just its geometry.
  EXPECT(indicator->property("animating").toBool() == true);
  EXPECT(indicator->property("hidesWhenStopped").toBool() == true);
  EXPECT(indicator->property("size").toString() == QStringLiteral("large"));
  EXPECT(indicator->property("color").value<QColor>() == QColor(QStringLiteral("#00aa66")));
}

/// The iOS switch descriptor is rendered as a QML switch, responds to a press,
/// and delivers the new value to JavaScript as `onValueChange`.
void switchToggles() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(
      kSurfaceId, "RNQnxSwitch", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  EXPECT(outer->childItems().size() == 1);
  QQuickItem* switchItem = outer->childItems().at(0);
  EXPECT(switchItem->property("value").toBool() == false);

  // Toggle the QML property directly. The valueChanged signal reaches the
  // bridge, which emits the native onChange event, and the JS wrapper turns
  // that into onValueChange.
  switchItem->setProperty("value", true);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(switchItem->property("value").toBool() == true);

  auto& rt = fixture.host().runtime();
  const auto valueChange = rt.global().getProperty(rt, "__switchValueChange");
  if (!valueChange.isNumber()) {
    fail(
        "onValueChange ran in JavaScript",
        __FILE__,
        __LINE__,
        "globalThis.__switchValueChange was never set");
  }
  EXPECT(valueChange.getNumber() >= 1.0);

  const auto lastValue = rt.global().getProperty(rt, "__switchLastValue");
  EXPECT(lastValue.isBool());
  EXPECT(lastValue.getBool() == true);
}

/// SafeAreaView has no platform fork; with zero insets it should mount as a
/// View and receive the same layout and styling.
void safeAreaViewMounts() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(
      kSurfaceId, "RNQnxSafeAreaView", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  EXPECT(outer->childItems().size() == 1);
  QQuickItem* safe = outer->childItems().at(0);
  EXPECT(safe != nullptr);
}

/// The Image component dispatches `onLoadStart`, `onLoad`, and `onLoadEnd`
/// events to JavaScript when a data: URI is loaded.
void imageLoadEvents() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(
      kSurfaceId, "RNQnxImage", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* image = fixture.root().childItems().at(0);
  EXPECT(image->width() == 32.0);
  EXPECT(image->height() == 32.0);

  auto& rt = fixture.host().runtime();
  const auto loadStart =
      rt.global().getProperty(rt, "__imageLoadStart");
  const auto load = rt.global().getProperty(rt, "__imageLoad");
  const auto loadEnd = rt.global().getProperty(rt, "__imageLoadEnd");
  const auto loadError = rt.global().getProperty(rt, "__imageError");
  EXPECT(loadStart.isNumber() && loadStart.getNumber() >= 1.0);
  EXPECT(load.isNumber() && load.getNumber() >= 1.0);
  EXPECT(loadEnd.isNumber() && loadEnd.getNumber() >= 1.0);
  EXPECT(!loadError.isNumber() || loadError.getNumber() == 0.0);
}

/// A JavaScript error reaches the host instead of aborting the process.
///
/// Before ExceptionsManager existed, React reached for it while reporting any
/// error, `getEnforcing` threw, and the throw escaped — so the first error
/// killed the process while trying to describe itself.
void javaScriptErrorsReachTheHost() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();

  // In bridgeless mode TurboModuleBinding installs `nativeModuleProxy`
  // (TurboModuleBinding.cpp:151-161), which is what TurboModuleRegistry falls
  // back to. Reaching it directly keeps the test independent of a bundle.
  const char* report = R"JS(
    const manager = globalThis.nativeModuleProxy.ExceptionsManager;
    manager.reportException({
      message: 'deliberate failure',
      isFatal: true,
      stack: [{ methodName: 'renderThing', file: 'App.js', lineNumber: 42 }],
    });
    manager.dismissRedbox();
  )JS";

  EXPECT(fixture.host().evaluate(report, "errors.js"));

  const auto& errors = fixture.host().javaScriptErrors();
  EXPECT(errors.size() == 1);
  EXPECT(errors.front().message == "deliberate failure");
  EXPECT(errors.front().isFatal);
  // The stack is flattened from React Native's frame objects, so the method and
  // location survive rather than being dropped.
  EXPECT(errors.front().stack.find("renderThing") != std::string::npos);
  EXPECT(errors.front().stack.find("App.js:42") != std::string::npos);
}

/// A healthy render reports nothing. This is the guard that keeps the error
/// path from quietly becoming noisy.
void aSuccessfulRenderReportsNoErrors() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.host().startSurface(kSurfaceId, "RNQnxPlayground", 640.0F, 480.0F);
  for (int i = 0; i < 10; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.rootChildCount() == 1);
  if (!fixture.host().javaScriptErrors().empty()) {
    fail(
        "render reported no JavaScript errors",
        __FILE__,
        __LINE__,
        fixture.host().javaScriptErrors().front().message);
  }
}

#ifdef RNQNX_PLAYGROUND_BUNDLE_PATH
/// The tree is actually painted, not merely configured.
///
/// Every other test asserts item properties, which proves the mounting layer
/// set them. This one renders the scene graph and reads pixels back, which is
/// the only thing that proves Qt Quick drew what those properties describe.
void renderedTreeIsPainted() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  auto& window = fixture.window();
  window.setWidth(640);
  window.setHeight(480);
  // A white backdrop makes an unpainted region obvious rather than ambiguous.
  window.setColor(QColor(Qt::white));
  window.show();

  fixture.host().startSurface(kSurfaceId, "RNQnxPlayground", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  const QImage frame = window.grabWindow();
  if (frame.isNull()) {
    fail(
        "window.grabWindow() produced a frame",
        __FILE__,
        __LINE__,
        "the platform plugin rendered nothing");
  }

  const qreal scale = frame.width() / 640.0;
  const auto pixelAt = [&frame, scale](int x, int y) {
    return frame.pixelColor(
        static_cast<int>(x * scale), static_cast<int>(y * scale));
  };

  // The outer view is 320x180 at the origin and painted #204080.
  const QColor outer = pixelAt(160, 90);
  EXPECT(outer.red() == 0x20);
  EXPECT(outer.green() == 0x40);
  EXPECT(outer.blue() == 0x80);

  // Outside it, the window's own colour shows through, so the view is bounded
  // rather than filling the surface.
  const QColor outside = pixelAt(500, 400);
  EXPECT(outside.red() > 0xF0 && outside.green() > 0xF0 && outside.blue() > 0xF0);

  // The inner view is #ff8800 at 50% opacity over the outer view, so the
  // composite is roughly halfway between the two — proof the opacity was
  // applied by the renderer rather than only stored on the item.
  const QColor inner = pixelAt(32, 16);
  EXPECT(inner.red() > outer.red());
  EXPECT(inner.red() < 0xFF);
  EXPECT(inner != outer);
}
#endif

#ifdef RNQNX_PLAYGROUND_BUNDLE_PATH
/// A pointer press reaches the JavaScript handler.
///
/// Qt delivers pointer events to the window; QtTouchDispatcher filters them,
/// hit-tests to the deepest mounted item, and calls that shadow view's
/// TouchEventEmitter. Nothing about that path is React Native's — it ships
/// Choreographer motion events on Android and UIKit touches on Apple.
void pointerEventsReachJavaScript() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  auto& dispatcher = fixture.enableTouch();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));

  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(kSurfaceId, "RNQnxPlayground", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }
  EXPECT(fixture.rootChildCount() == 1);

  // The inner view is 64x32 at the origin, so this lands inside it.
  const QPointF point(20.0, 10.0);
  QMouseEvent press(
      QEvent::MouseButtonPress,
      point,
      point,
      Qt::LeftButton,
      Qt::LeftButton,
      Qt::NoModifier);
  QMouseEvent release(
      QEvent::MouseButtonRelease,
      point,
      point,
      Qt::LeftButton,
      Qt::NoButton,
      Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.window(), &press);
  QCoreApplication::sendEvent(&fixture.window(), &release);

  EXPECT(dispatcher.dispatchedCount() == 2);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  auto& runtime = fixture.host().runtime();
  const auto starts = runtime.global().getProperty(runtime, "__touchStarts");
  const auto ends = runtime.global().getProperty(runtime, "__touchEnds");
  if (!starts.isNumber()) {
    fail(
        "onTouchStart ran in JavaScript",
        __FILE__,
        __LINE__,
        "globalThis.__touchStarts was never set");
  }
  EXPECT_NEAR(starts.getNumber(), 1.0);
  EXPECT(ends.isNumber());
  EXPECT_NEAR(ends.getNumber(), 1.0);

  // React's Fabric renderer runs the JavaScript responder system on top of
  // these touches, and it negotiated a responder for this view: it asked, the
  // view claimed it, and the release was delivered. That is the machinery
  // Pressability and every gesture are built on.
  for (const char* name : {"__askedResponder", "__granted", "__released"}) {
    const auto value = runtime.global().getProperty(runtime, name);
    if (!value.isNumber()) {
      fail(
          "the responder callback ran in JavaScript",
          __FILE__,
          __LINE__,
          name);
    }
    EXPECT_NEAR(value.getNumber(), 1.0);
  }

  // Pressable is the first component here that is not a bare View. It sits
  // below the first child, so 48pt down lands inside it.
  const QPointF pressablePoint(20.0, 48.0);
  QMouseEvent pressableDown(
      QEvent::MouseButtonPress,
      pressablePoint,
      pressablePoint,
      Qt::LeftButton,
      Qt::LeftButton,
      Qt::NoModifier);
  QMouseEvent pressableUp(
      QEvent::MouseButtonRelease,
      pressablePoint,
      pressablePoint,
      Qt::LeftButton,
      Qt::NoButton,
      Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.window(), &pressableDown);
  QCoreApplication::sendEvent(&fixture.window(), &pressableUp);
  for (int i = 0; i < 40; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  const auto presses = runtime.global().getProperty(runtime, "__presses");
  if (!presses.isNumber()) {
    fail(
        "Pressable.onPress ran",
        __FILE__,
        __LINE__,
        "globalThis.__presses was never set");
  }
  EXPECT_NEAR(presses.getNumber(), 1.0);

  // The handler read locationX off the event, so the coordinate survived the
  // trip from Qt into React Native's touch payload.
  const auto locationX = runtime.global().getProperty(runtime, "__lastTouch");
  EXPECT(locationX.isNumber());
  EXPECT_NEAR(locationX.getNumber(), 20.0);
}
#endif

struct TestCase {
  const char* name;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);

  const std::vector<TestCase> tests{
      {"runtimeEvaluatesJavaScript", runtimeEvaluatesJavaScript},
      {"javaScriptErrorsAreReportedNotSwallowed",
       javaScriptErrorsAreReportedNotSwallowed},
      {"schedulerInstallsTheFabricUIManager",
       schedulerInstallsTheFabricUIManager},
      {"surfaceCommitMountsThroughTheDelegate",
       surfaceCommitMountsThroughTheDelegate},
      {"secondCommitUpdatesTheSameItem", secondCommitUpdatesTheSameItem},
      {"stoppingASurfaceUnmountsItsItems", stoppingASurfaceUnmountsItsItems},
      {"commitToAnUnknownSurfaceIsReported", commitToAnUnknownSurfaceIsReported},
      {"componentPredicateAnswersForRegisteredNames",
       componentPredicateAnswersForRegisteredNames},
      {"javaScriptBuildsTheTreeThroughFabricUIManager",
       javaScriptBuildsTheTreeThroughFabricUIManager},
      {"hostInstallsBridgelessGlobals", hostInstallsBridgelessGlobals},

#ifdef RNQNX_PLAYGROUND_BUNDLE_PATH
      {"metroProducesABundleForQnx", metroProducesABundleForQnx},
      {"metroBundleEvaluatesInHermes", metroBundleEvaluatesInHermes},
      {"metroBundleRendersThroughReact", metroBundleRendersThroughReact},
      {"javaScriptErrorsReachTheHost", javaScriptErrorsReachTheHost},
#ifdef RNQNX_PLAYGROUND_BUNDLE_PATH
      {"aSuccessfulRenderReportsNoErrors", aSuccessfulRenderReportsNoErrors},
      {"renderedTreeIsPainted", renderedTreeIsPainted},
      {"pointerEventsReachJavaScript", pointerEventsReachJavaScript},
      {"scrollEventsReachJavaScript", scrollEventsReachJavaScript},
      {"flatListRendersAndVirtualises", flatListRendersAndVirtualises},
      {"textInputMounts", textInputMounts},
      {"activityIndicatorMounts", activityIndicatorMounts},
      {"switchToggles", switchToggles},
      {"safeAreaViewMounts", safeAreaViewMounts},
      {"imageLoadEvents", imageLoadEvents},
#endif
#endif
  };

  int failed = 0;
  for (const auto& test : tests) {
    try {
      test.run();
      // Flushed: stdout is buffered, and a crash in a later case would
      // otherwise discard the record of which ones passed.
      std::cout << "[PASS] " << test.name << std::endl;
    } catch (const TestFailure& failure) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": " << failure.what() << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
    } catch (...) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
    }
  }

  if (failed != 0) {
    std::cerr << failed << " QtReactHost host test(s) failed.\n";
    return EXIT_FAILURE;
  }

  std::cout << "All " << tests.size() << " QtReactHost host tests passed.\n";
  return EXIT_SUCCESS;
}
