#include "react-native-qnx/views/QtTextInputQml.h"
/*
 * Host tests for the runtime/renderer wiring: a Hermes runtime, a
 * RuntimeScheduler, React Native's Scheduler with our EventBeat, component
 * registry factory and SchedulerDelegate, and a surface whose commits land on
 * Qt Quick items.
 *
 * These are the five pieces React Native ships no generic implementation of.
 * Host evidence only: JavaScript runs on the test thread. The full lane loads
 * the real Metro playground bundle. None of this is QNX cross-build/device
 * evidence.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <react/renderer/components/view/ViewShadowNode.h>
#include <react/renderer/core/RawEvent.h>
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
#include <QTouchEvent>
#include <QPointingDevice>
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
#include <thread>
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
constexpr const char* kViewSource =
    "import QtQuick\n"
    "Rectangle { id: root; visible: true; property alias backgroundColor: root.color }\n";
constexpr const char* kTextSource = "import QtQuick\nText { visible: true }\n";
constexpr const char* kImageSource =
    "import QtQuick\n"
    "Image {\n"
    "  id: root\n"
    "  visible: true\n"
    "  property var bridge: null\n"
    "  property string loadingSource: \"\"\n"
    "  Image {\n"
    "    id: loadingImage\n"
    "    objectName: \"loadingImage\"\n"
    "    anchors.fill: parent\n"
    "    source: root.loadingSource\n"
    "    visible: root.loadingSource !== \"\" &&\n"
    "             root.status !== Image.Ready &&\n"
    "             root.status !== Image.Error\n"
    "    z: -1\n"
    "  }\n"
    "  onStatusChanged: { if (bridge) bridge.notify(); }\n"
    "}\n";
// Flickable is Qt Quick's scrollable container; clip keeps content inside the
// viewport, which is what a React Native ScrollView does.
constexpr const char* kScrollViewSource =
    "import QtQuick\nFlickable { visible: true; clip: true }\n";
// React Native's TextInput renders behind a single-line QML `TextInput` or a
// multi-line `TextEdit`, both hosted in an `Item` that provides a `text`
// property, `textChanged` signal, and an overlaid placeholder `Text`.
const char* kTextInputSource = rnqnx::views::textInputQml();
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
  explicit Fixture(bool enableInputLifecycle = RNQNX_ENABLE_INPUT_LIFECYCLE
#if RNQNX_ENABLE_FRAME_TRACE
                   , std::shared_ptr<rnqnx::runtime::RuntimeTrace> trace = nullptr
#endif
                   )
      : factory_(engine_),
        layer_(factory_, *window_.contentItem(), kSurfaceId),
        adapter_(layer_),
        dispatcher_(layer_, *window_.contentItem(), enableInputLifecycle),
#if RNQNX_ENABLE_FRAME_TRACE
        host_(adapter_, std::move(trace)),
#else
        host_(adapter_),
#endif
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
    adapter_.mapComponent(
        QStringLiteral("ModalHostView"), QStringLiteral("View"));
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
  // Deterministic injected input: bypass QQuickWindow's secondary mouse
  // synthesis while retaining the real Qt filter and Fabric event queue.
  QObject& touchInputSource() {
    touchInputSource_.installEventFilter(&dispatcher_);
    return touchInputSource_;
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
  QObject touchInputSource_;
};

#if RNQNX_ENABLE_FRAME_TRACE
void runtimeTraceFollowsOwnedHostBoundaries() {
  using rnqnx::runtime::RuntimeTrace;
  using rnqnx::runtime::TracePhase;
  using rnqnx::runtime::TraceStage;
  auto trace = std::make_shared<RuntimeTrace>(1000);
  for (int trial = 0; trial < 2; ++trial) {
    Fixture fixture(RNQNX_ENABLE_INPUT_LIFECYCLE, trace);
    EXPECT(fixture.host().evaluate("1 + 1", "private-source-url-must-not-be-exported"));
    fixture.host().startEmptySurface(kSurfaceId, 640.0F, 480.0F);
    auto called = std::make_shared<bool>(false);
    auto queue = fixture.host().taskQueue();
    std::thread worker([queue, called] { queue->post([called] { *called = true; }); });
    worker.join();
    for (int i = 0; i < 20 && !*called; ++i) fixture.host().pumpEvents();
    EXPECT(*called);
    fixture.host().stopSurface(kSurfaceId);
  }
  const auto snapshot = trace->freeze();
  EXPECT(snapshot.dropped == 0);
  std::vector<std::uint64_t> generations;
  bool evaluate = false;
  bool executed = false;
  bool closed = false;
  for (const auto& event : snapshot.events) {
    if (event.stage == TraceStage::SurfaceStart && event.phase == TracePhase::Begin) {
      EXPECT(event.surface == kSurfaceId);
      generations.push_back(event.surfaceGeneration);
    }
    evaluate = evaluate || event.stage == TraceStage::Evaluate;
    executed = executed || event.stage == TraceStage::QueueExecute;
    closed = closed || (event.stage == TraceStage::QueueClose && event.phase == TracePhase::End);
  }
  EXPECT(generations.size() == 2 && generations[0] != 0 && generations[0] != generations[1]);
  EXPECT(evaluate && executed && closed);
  std::ostringstream exported;
  RuntimeTrace::writeJson(snapshot, exported);
  EXPECT(exported.str().find("private-source-url") == std::string::npos);
}
#endif

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

void evaluateRejectsReentrantJavaScript() {
  Fixture fixture;
  auto& runtime = fixture.host().runtime();
  runtime.global().setProperty(
      runtime, "tryNestedEvaluate",
      facebook::jsi::Function::createFromHostFunction(
          runtime, facebook::jsi::PropNameID::forAscii(runtime, "tryNestedEvaluate"), 0,
          [&fixture](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                     const facebook::jsi::Value*, size_t) {
            return facebook::jsi::Value(fixture.host().evaluate(
                "globalThis.nestedSideEffect = true", "nested.js"));
          }));
  EXPECT(fixture.host().evaluate(
      "globalThis.nestedResult = tryNestedEvaluate()", "outer.js"));
  EXPECT(!runtime.global().getProperty(runtime, "nestedResult").getBool());
  EXPECT(runtime.global().getProperty(runtime, "nestedSideEffect").isUndefined());
}

void timerCallbacksRunMicrotaskCheckpoints() {
  Fixture fixture;
  fixture.host().installTimers();
  EXPECT(fixture.host().evaluate(
      "setTimeout(() => Promise.resolve().then(() => {"
      "globalThis.timerMicrotaskDone = true; }), 0)", "timer-task.js"));
  auto& runtime = fixture.host().runtime();
  QElapsedTimer deadline;
  deadline.start();
  while (deadline.elapsed() < 1000 &&
         runtime.global().getProperty(runtime, "timerMicrotaskDone").isUndefined()) {
    // Intentionally do not drainMicrotasks here. Timer entry must do that via
    // RN's scheduler, just as a running application expects.
    fixture.host().pumpEvents();
  }
  const auto done = runtime.global().getProperty(runtime, "timerMicrotaskDone");
  EXPECT(done.isBool() && done.getBool());
}

void throwingMicrotasksAreReportedAndRecover() {
  Fixture fixture;
  EXPECT(!fixture.host().evaluate(
      "HermesInternal.enqueueJob(() => { throw new Error('checkpoint-boom'); });",
      "checkpoint-error.js"));
  EXPECT(fixture.host().lastError().contains(QStringLiteral("checkpoint-boom")));
  EXPECT(fixture.host().javaScriptErrors().size() == 1);
  EXPECT(fixture.host().javaScriptErrors().front().isFatal);
  EXPECT(fixture.host().evaluate("globalThis.recovered = true", "recovery.js"));
  EXPECT(fixture.host().lastError().isEmpty());
  auto& runtime = fixture.host().runtime();
  EXPECT(runtime.global().getProperty(runtime, "recovered").getBool());
}

void throwingTimerCallbacksAreReportedAndRecover() {
  Fixture fixture;
  fixture.host().installTimers();
  EXPECT(fixture.host().evaluate(
      "setTimeout(() => { throw new Error('timer-boom'); }, 0)", "timer-error.js"));
  QElapsedTimer deadline;
  deadline.start();
  while (fixture.host().javaScriptErrors().empty() && deadline.elapsed() < 1000) {
    fixture.host().pumpEvents();
  }
  EXPECT(fixture.host().javaScriptErrors().size() == 1);
  EXPECT(fixture.host().lastError().contains(QStringLiteral("timer-boom")));
  EXPECT(fixture.host().evaluate("1 + 1", "timer-recovery.js"));
  EXPECT(fixture.host().lastError().isEmpty());
}

void schedulerErrorsPreserveAndContainJavaScriptHandlers() {
  Fixture fixture;
  EXPECT(!fixture.host().evaluate(
      "globalThis.ErrorUtils = {reportError() {}, reportFatalError(error) {"
      "globalThis.forwardedError = error.message; }};"
      "HermesInternal.enqueueJob(() => { throw new Error('hook-boom'); });",
      "error-hook.js"));
  auto& runtime = fixture.host().runtime();
  EXPECT(runtime.global().getProperty(runtime, "forwardedError")
             .asString(runtime).utf8(runtime) == "hook-boom");
  EXPECT(fixture.host().javaScriptErrors().size() == 1);
  EXPECT(!fixture.host().evaluate(
      "ErrorUtils.reportFatalError = () => { throw new Error('handler-boom'); };"
      "HermesInternal.enqueueJob(() => { throw new Error('original-boom'); });",
      "broken-error-hook.js"));
  EXPECT(fixture.host().lastError().contains(QStringLiteral("original-boom")));
  EXPECT(fixture.host().javaScriptErrors().size() == 2);
  EXPECT(fixture.host().evaluate("1 + 1", "hook-recovery.js"));
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

  // A surface is a fixed viewport even when its only root child uses flex and
  // percentages. The child's fixed-size descendants keep their own Yoga size.
  const std::string fillScript = R"JS({
    const fabric = globalThis.nativeFabricUIManager;
    const surfaceId = )JS" + std::to_string(kSurfaceId) + R"JS(;
    const fillingRoot = fabric.createNode(502, 'View', surfaceId,
      { flex: 1, width: '100%', height: '100%', collapsable: false }, {});
    const fixedChild = fabric.createNode(503, 'View', surfaceId,
      { width: 120, height: 60, collapsable: false }, {});
    fabric.appendChild(fillingRoot, fixedChild);
    const fillingSet = fabric.createChildSet();
    fabric.appendChildToSet(fillingSet, fillingRoot);
    fabric.completeRoot(surfaceId, fillingSet);
  })JS";
  if (!fixture.host().evaluate(fillScript, "surface-viewport.js")) {
    fail("viewport script evaluated", __FILE__, __LINE__,
         fixture.host().lastError().toStdString());
  }
  EXPECT(fixture.host().delegate().lastFailures().empty());
  QQuickItem* filling = fixture.layer().itemForTag(502);
  QQuickItem* fixed = fixture.layer().itemForTag(503);
  EXPECT(filling != nullptr);
  EXPECT(fixed != nullptr);
  EXPECT_NEAR(filling->width(), 640.0);
  EXPECT_NEAR(filling->height(), 480.0);
  EXPECT_NEAR(fixed->width(), 120.0);
  EXPECT_NEAR(fixed->height(), 60.0);
}

// Drive the existing Fabric event binding without a Metro bundle. These tests
// inspect real serialized TouchEvent payloads, not dispatcher implementation state.
void mountTouchFixture(Fixture& fixture) {
  fixture.host().installComponentProvider();
  fixture.enableTouch();
  fixture.window().resize(640, 480);
  fixture.window().show();
  fixture.host().startEmptySurface(kSurfaceId, 640.0F, 480.0F);
  const std::string script = R"JS(
    globalThis.touchEvents = [];
    var fabric = globalThis.nativeFabricUIManager;
    fabric.registerEventHandler((instance, type, payload) => {
      if (type.startsWith('topTouch')) touchEvents.push({type, payload});
    });
    var surfaceId = )JS" + std::to_string(kSurfaceId) + R"JS(;
    var node = fabric.createNode(501, 'View', surfaceId,
      {width: 160, height: 80, collapsable: false}, {});
    const childSet = fabric.createChildSet();
    fabric.appendChildToSet(childSet, node);
    fabric.completeRoot(surfaceId, childSet);
  )JS";
  EXPECT(fixture.host().evaluate(script, "touch-fixture.js"));
  EXPECT(fixture.layer().itemForTag(501) != nullptr);
}

void flushTouchEvents(Fixture& fixture) {
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }
}

void sendTouch(
    Fixture& fixture,
    QEvent::Type type,
    const QList<QEventPoint>& points,
    ulong timestamp = 100) {
  static const QPointingDevice touchscreen(
      QStringLiteral("dispatcher-test-touchscreen"), 17,
      QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
      QInputDevice::Capability::Position, 10, 0);
  QTouchEvent event(type, &touchscreen, Qt::NoModifier, points);
  event.setTimestamp(timestamp);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &event);
  flushTouchEvents(fixture);
}

void inputLifecycleOptOutPreservesExistingDispatch() {
  Fixture fixture(false);
  mountTouchFixture(fixture);
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {220, 310})});
  sendTouch(fixture, QEvent::TouchCancel, {});
  sendTouch(fixture, QEvent::TouchEnd, {
      QEventPoint(7, QEventPoint::Released, {40, 20}, {240, 320})});
  EXPECT(fixture.host().evaluate(R"JS(
    if (touchEvents.length !== 2 || touchEvents[0].type !== 'topTouchStart' ||
        touchEvents[1].type !== 'topTouchEnd' ||
        touchEvents[1].payload.changedTouches[0].pageX !== 20 ||
        touchEvents[0].payload.changedTouches[0].screenX !== 20 ||
        touchEvents[1].payload.changedTouches[0].force !== 1)
      throw new Error('opt-out changed the existing input contract');
  )JS", "touch-opt-out-check.js"));
}

void touchCancelEndsEveryContactAndAllowsIdReuse() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10}),
      QEventPoint(8, QEventPoint::Pressed, {30, 15}, {30, 15})});
  // Qt can cancel a stream with no points. Use stored contacts, then clear all
  // of them before sending any cancellation so touches is consistently empty.
  sendTouch(fixture, QEvent::TouchCancel, {});
  sendTouch(fixture, QEvent::TouchCancel, {});
  sendTouch(fixture, QEvent::TouchEnd, {
      QEventPoint(7, QEventPoint::Released, {40, 20}, {40, 20})});
  EXPECT(fixture.host().evaluate(R"JS(
    const cancels = touchEvents.filter(e => e.type === 'topTouchCancel');
    if (cancels.length !== 2 || cancels.some(e =>
        e.payload.touches.length || e.payload.targetTouches.length))
      throw new Error('cancel must terminate each contact exactly once');
    if (touchEvents.some(e => e.type === 'topTouchEnd'))
      throw new Error('cancelled contact produced a duplicate end');
  )JS", "touch-cancel-check.js"));
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10})});
  sendTouch(fixture, QEvent::TouchEnd, {
      QEventPoint(7, QEventPoint::Released, {25, 12}, {25, 12})});
  EXPECT(fixture.host().evaluate(R"JS(
    if (touchEvents.filter(e => e.type === 'topTouchStart').length !== 3 ||
        touchEvents.filter(e => e.type === 'topTouchEnd').length !== 1)
      throw new Error('contact ID could not be reused after cancellation');
  )JS", "touch-reuse-check.js"));
}

void touchReleaseUsesTerminalCoordinatesAndKeepsOtherContact() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10}),
      QEventPoint(8, QEventPoint::Pressed, {30, 15}, {30, 15})});
  sendTouch(fixture, QEvent::TouchUpdate, {
      QEventPoint(7, QEventPoint::Released, {60, 35}, {60, 35}),
      QEventPoint(8, QEventPoint::Stationary, {30, 15}, {30, 15})}, 250);
  EXPECT(fixture.host().evaluate(R"JS(
    const end = touchEvents.find(e => e.type === 'topTouchEnd').payload;
    const touch = end.changedTouches[0];
    if (touch.identifier !== 7 || touch.pageX !== 60 || touch.pageY !== 35 ||
        touch.locationX !== 60 || touch.locationY !== 35 || touch.timestamp !== 250)
      throw new Error('release retained stale position or timestamp');
    if (end.touches.length !== 1 || end.touches[0].identifier !== 8 ||
        end.targetTouches.length !== 1)
      throw new Error('release removed the wrong active contacts');
  )JS", "touch-release-check.js"));
}

void forgettingTouchTargetCancelsAndClearsResponder() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  auto& dispatcher = fixture.enableTouch();
  EXPECT(fixture.host().evaluate(R"JS(
    const sibling = fabric.createNode(502, 'View', surfaceId,
      {width: 160, height: 80, collapsable: false}, {});
    const twoChildren = fabric.createChildSet();
    fabric.appendChildToSet(twoChildren, node);
    fabric.appendChildToSet(twoChildren, sibling);
    fabric.completeRoot(surfaceId, twoChildren);
  )JS", "touch-sibling.js"));
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10}),
      QEventPoint(8, QEventPoint::Pressed, {20, 90}, {20, 90})});
  dispatcher.setJsResponder(501, true);
  dispatcher.forgetTag(501);
  flushTouchEvents(fixture);
  EXPECT(fixture.host().evaluate(R"JS(
    const cancelled = touchEvents.filter(e => e.type === 'topTouchCancel');
    if (cancelled.length !== 1 || cancelled[0].payload.changedTouches[0].identifier !== 7)
      throw new Error('unmount must cancel only the removed target');
    const remaining = cancelled[0].payload.touches;
    if (remaining.length !== 1 || remaining[0].identifier !== 8 ||
        remaining[0].target !== 502 || cancelled[0].payload.targetTouches.length)
      throw new Error('unmount removed another target contact');
    const f = globalThis.nativeFabricUIManager;
    const replacementNode = f.createNode(503, 'View', )JS" + std::to_string(kSurfaceId) + R"JS(,
      {width: 160, height: 80, collapsable: false}, {});
    const children = f.createChildSet();
    f.appendChildToSet(children, replacementNode);
    f.completeRoot()JS" + std::to_string(kSurfaceId) + R"JS(, children);
  )JS", "touch-unmount-check.js"));
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(9, QEventPoint::Pressed, {20, 10}, {20, 10})});
  EXPECT(fixture.host().evaluate(R"JS(
    const starts = touchEvents.filter(e => e.type === 'topTouchStart');
    if (starts.length !== 3 || starts[2].payload.changedTouches[0].target !== 503)
      throw new Error('removed responder captured the next gesture');
  )JS", "touch-next-target-check.js"));
}

void windowDeactivationCancelsTouchStream() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  auto& dispatcher = fixture.enableTouch();
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10})});
  dispatcher.setJsResponder(501, true);
  QEvent deactivate(QEvent::WindowDeactivate);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &deactivate);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &deactivate);
  sendTouch(fixture, QEvent::TouchEnd, {
      QEventPoint(7, QEventPoint::Released, {40, 20}, {40, 20})});
  // This falls outside the view. A stale responder would capture it anyway.
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(9, QEventPoint::Pressed, {300, 300}, {300, 300})});
  EXPECT(fixture.host().evaluate(R"JS(
    if (touchEvents.length !== 2 || touchEvents[1].type !== 'topTouchCancel' ||
        touchEvents[1].payload.touches.length !== 0)
      throw new Error('deactivation must cancel once and release the responder');
  )JS", "touch-deactivate-check.js"));
}

void mouseCaptureCancellationDoesNotCreateHoverTouches() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  auto& dispatcher = fixture.enableTouch();
  dispatcher.setJsResponder(501, true);
  const QPointF point(20, 10);
  QMouseEvent hover(QEvent::MouseMove, point, point,
                    Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &hover);
  EXPECT(dispatcher.dispatchedCount() == 0);
  QMouseEvent press(QEvent::MouseButtonPress, point, point,
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &press);
  QEvent ungrab(QEvent::UngrabMouse);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &ungrab);
  QMouseEvent release(QEvent::MouseButtonRelease, point, point,
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &release);
  // A stale JS responder must not redirect a new press outside all views.
  const QPointF outside(300, 300);
  QMouseEvent nextPress(QEvent::MouseButtonPress, outside, outside,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &nextPress);
  flushTouchEvents(fixture);
  EXPECT(fixture.host().evaluate(R"JS(
    if (touchEvents.length !== 2 || touchEvents[0].type !== 'topTouchStart' ||
        touchEvents[1].type !== 'topTouchCancel' ||
        touchEvents[1].payload.touches.length !== 0)
      throw new Error('lost mouse capture must cancel once without hover/end');
  )JS", "mouse-cancel-check.js"));
}

void mouseReleaseClearsCaptureBeforeNativeListeners() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  auto& dispatcher = fixture.enableTouch();
  bool ended = false;
  auto listener = std::make_shared<const facebook::react::EventListener>(
      [&](const facebook::react::RawEvent& event) {
        if (event.type == "topTouchEnd") {
          ended = true;
          dispatcher.forgetTag(501);
        }
        return false;
      });
  fixture.host().scheduler().addEventListener(listener);
  const QPointF point(20, 10);
  QMouseEvent press(QEvent::MouseButtonPress, point, point,
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QMouseEvent release(QEvent::MouseButtonRelease, point, point,
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &press);
  QCoreApplication::sendEvent(&fixture.touchInputSource(), &release);
  fixture.host().scheduler().removeEventListener(listener);
  EXPECT(ended);
  flushTouchEvents(fixture);
  EXPECT(fixture.host().evaluate(R"JS(
    if (touchEvents.length !== 2 || touchEvents[1].type !== 'topTouchEnd')
      throw new Error('release listener removal emitted an extra cancellation');
  )JS", "mouse-release-removal.js"));
}

void decoratedViewTargetsNearestRegisteredAncestor() {
  Fixture fixture(true);
  mountTouchFixture(fixture);
  auto* view = fixture.layer().itemForTag(501);
  // Real View backgrounds/borders contain unregistered QML children.
  QQuickItem decoration(view);
  decoration.setWidth(160);
  decoration.setHeight(80);
  QQuickItem nestedDecoration(&decoration);
  nestedDecoration.setWidth(160);
  nestedDecoration.setHeight(80);
  sendTouch(fixture, QEvent::TouchBegin, {
      QEventPoint(7, QEventPoint::Pressed, {20, 10}, {20, 10})});
  sendTouch(fixture, QEvent::TouchEnd, {
      QEventPoint(7, QEventPoint::Released, {20, 10}, {20, 10})});
  EXPECT(fixture.host().evaluate(R"JS(
    if (touchEvents.length !== 2 || touchEvents.some(e =>
        e.payload.changedTouches[0].target !== 501))
      throw new Error('unregistered decoration swallowed its View target');
  )JS", "decorated-view-touch.js"));
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


/// Platform constants must track the React Native line this repository builds.
void platformConstantsReportCurrentReactNativeVersion() {
  Fixture fixture;
  fixture.host().installPlatformConstants();

  auto& runtime = fixture.host().runtime();
  const auto constantsValue =
      runtime.global().getProperty(runtime, "__RNQNX_PLATFORM_CONSTANTS");
  EXPECT(constantsValue.isObject());
  const auto constants = constantsValue.getObject(runtime);

  const auto versionValue = constants.getProperty(runtime, "reactNativeVersion");
  EXPECT(versionValue.isObject());
  const auto version = versionValue.getObject(runtime);

  EXPECT_NEAR(version.getProperty(runtime, "major").getNumber(), 0.0);
  EXPECT_NEAR(version.getProperty(runtime, "minor").getNumber(), 88.0);
  EXPECT_NEAR(version.getProperty(runtime, "patch").getNumber(), 1.0);
  const auto prerelease = version.getProperty(runtime, "prerelease");
  EXPECT(prerelease.isNull());
}

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

  // The momentum and drag events are also wired through the Flickable's
  // signals. Directly invoking those signals proves the event path without
  // needing real input events in the host test.
  QMetaObject::invokeMethod(
      scroll, "dragStarted", Qt::DirectConnection);
  QMetaObject::invokeMethod(
      scroll, "dragEnded", Qt::DirectConnection);
  QMetaObject::invokeMethod(
      scroll, "flickStarted", Qt::DirectConnection);
  QMetaObject::invokeMethod(
      scroll, "flickEnded", Qt::DirectConnection);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  const auto beginDrag =
      runtime.global().getProperty(runtime, "__scrollBeginDrag");
  EXPECT(beginDrag.isNumber());
  EXPECT(beginDrag.getNumber() >= 1.0);

  const auto endDrag =
      runtime.global().getProperty(runtime, "__scrollEndDrag");
  EXPECT(endDrag.isNumber());
  EXPECT(endDrag.getNumber() >= 1.0);

  const auto momentumBegin =
      runtime.global().getProperty(runtime, "__momentumScrollBegin");
  EXPECT(momentumBegin.isNumber());
  EXPECT(momentumBegin.getNumber() >= 1.0);

  const auto momentumEnd =
      runtime.global().getProperty(runtime, "__momentumScrollEnd");
  EXPECT(momentumEnd.isNumber());
  EXPECT(momentumEnd.getNumber() >= 1.0);
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

/// SectionList is upstream JavaScript over VirtualizedList, so it should
/// render if VirtualizedList, ScrollView, View, and Text all work.
void sectionListRenders() {
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
      kSurfaceId, "RNQnxSectionList", 640.0F, 480.0F);
  for (int i = 0; i < 40; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  auto& rt = fixture.host().runtime();
  const auto viewable =
      rt.global().getProperty(rt, "__sectionListViewable");
  if (!viewable.isNumber()) {
    fail(
        "SectionList did not report viewable items",
        __FILE__,
        __LINE__,
        "globalThis.__sectionListViewable was never set");
  }
  EXPECT(viewable.getNumber() >= 1.0);
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

  auto& rt = fixture.host().runtime();

  const auto focus = rt.global().getProperty(rt, "__inputFocus");
  EXPECT(focus.isNumber());
  EXPECT(focus.getNumber() >= 1.0);
  const auto focusText = rt.global().getProperty(rt, "__inputFocusText");
  EXPECT(focusText.isString());
  EXPECT(focusText.getString(rt).utf8(rt) == "typed");

  QKeyEvent key(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("X"));
  QCoreApplication::sendEvent(&fixture.window(), &key);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(input->property("text").toString() == QStringLiteral("typedX"));

  const auto keyPressCount = rt.global().getProperty(rt, "__inputKeyPress");
  EXPECT(keyPressCount.isNumber());
  EXPECT(keyPressCount.getNumber() >= 1.0);
  const auto lastKey = rt.global().getProperty(rt, "__inputLastKey");
  EXPECT(lastKey.isString());
  EXPECT(lastKey.getString(rt).utf8(rt) == "X");

  const auto selection = rt.global().getProperty(rt, "__inputSelection");
  EXPECT(selection.isObject());
  const auto selectionObj = selection.getObject(rt);
  EXPECT(selectionObj.getProperty(rt, "start").asNumber() == (RNQNX_ENABLE_TEXT_INPUT_CONTRACT ? 6.0 : 5.0));
  EXPECT(selectionObj.getProperty(rt, "end").asNumber() == (RNQNX_ENABLE_TEXT_INPUT_CONTRACT ? 6.0 : 5.0));

  QKeyEvent returnKey(
      QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
  QCoreApplication::sendEvent(&fixture.window(), &returnKey);

  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  const auto submit = rt.global().getProperty(rt, "__inputSubmit");
  EXPECT(submit.isNumber());
  EXPECT(submit.getNumber() >= 1.0);
  const auto submitText = rt.global().getProperty(rt, "__inputSubmitText");
  EXPECT(submitText.isString());
  EXPECT(submitText.getString(rt).utf8(rt) == "typedX");
  const auto lastKeyEnter = rt.global().getProperty(rt, "__inputLastKey");
  EXPECT(lastKeyEnter.isString());
  EXPECT(lastKeyEnter.getString(rt).utf8(rt) == "Enter");

  input->setFocus(false);
  for (int i = 0; i < 5; ++i) {
    fixture.host().pumpEvents();
  }
  const auto blur = rt.global().getProperty(rt, "__inputBlur");
  EXPECT(blur.isNumber());
  EXPECT(blur.getNumber() >= 1.0);

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

/// `TextInput` with a `placeholder` and `placeholderTextColor` applies both to
/// the underlying QML item.
void textInputPlaceholder() {
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
  fixture.host().startSurface(kSurfaceId, "RNQnxTextInput", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  QQuickItem* input = outer->childItems().at(0);
  EXPECT(input->property("placeholderText").toString() ==
         QStringLiteral("enter text"));
  EXPECT(input->property("multiline").toBool() == false);
}

/// `TextInput` with `multiline: true` loads the `TextEdit` component.
void textInputMultiline() {
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
      kSurfaceId, "RNQnxTextInputMultiline", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* outer = fixture.root().childItems().at(0);
  QQuickItem* input = outer->childItems().at(0);
  EXPECT(input->property("multiline").toBool() == true);
  EXPECT(input->property("placeholderText").toString() ==
         QStringLiteral("type multiple lines"));
  EXPECT(input->property("text").toString() ==
         QStringLiteral("line one"));
}

/// `Text` with `numberOfLines` applies `elide` and `wrapMode` to the QML item.
void textEllipsis() {
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
      kSurfaceId, "RNQnxTextEllipsis", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* root = fixture.root().childItems().at(0);
  EXPECT(root->childItems().size() == 1);
  QQuickItem* text = root->childItems().at(0);
  EXPECT(text->property("elide").toInt() == 1);  // Text.ElideRight
  EXPECT(text->property("wrapMode").toInt() == 0);  // Text.NoWrap
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

/// Button is pure upstream JavaScript over View, Text and the touch system.
/// It needs `NativeAnimated` because `TouchableOpacity` drives native opacity.
void buttonMounts() {
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
      kSurfaceId, "RNQnxButton", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);
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

/// View `onLayout` reaches JavaScript with the laid-out frame.
void viewLayoutFires() {
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
  fixture.host().startSurface(kSurfaceId, "RNQnxViewLayout", 640.0F, 480.0F);
  for (int i = 0; i < 40; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  auto& rt = fixture.host().runtime();
  const auto layout = rt.global().getProperty(rt, "__viewLayout");
  EXPECT(layout.isObject());
  const auto layoutObj = layout.getObject(rt);
  EXPECT(layoutObj.getProperty(rt, "width").asNumber() == 120.0);
  EXPECT(layoutObj.getProperty(rt, "height").asNumber() == 60.0);
}

/// View `transform` (translate, rotate, scale) reaches the Qt item.
void viewTransformApplies() {
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
      kSurfaceId, "RNQnxTransform", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* transformed = fixture.root().childItems().at(0);
  EXPECT(transformed != nullptr);

  EXPECT_NEAR(transformed->x(), 70.0);
  EXPECT_NEAR(transformed->y(), 70.0);
  EXPECT_NEAR(transformed->scale(), 1.5);
  EXPECT_NEAR(transformed->rotation(), 45.0);
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
  EXPECT(image->property("fillMode").toInt() == 1);

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

/// `Image` `defaultSource` is used when no primary source is provided.
void imageDefaultSourceFires() {
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
      kSurfaceId, "RNQnxImageDefault", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  auto& rt = fixture.host().runtime();
  const auto load = rt.global().getProperty(rt, "__imageDefaultLoad");
  EXPECT(load.isNumber());
  EXPECT(load.getNumber() >= 1.0);
  const auto uri = rt.global().getProperty(rt, "__imageDefaultUri");
  EXPECT(uri.isString());
}

/// `Image` `loadingIndicatorSource` is shown while the primary source loads and
/// the primary source's load events still reach JavaScript.
void imageLoadingIndicator() {
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
      kSurfaceId, "RNQnxImageLoading", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* image = fixture.root().childItems().at(0);
  EXPECT(image->width() == 32.0);
  EXPECT(image->height() == 32.0);

  // The overlay item for the loading indicator should exist and should have
  // a valid source while the primary image is loading or completed.
  QQuickItem* loadingImage = image->findChild<QQuickItem*>("loadingImage");
  // It may be hidden once the primary source is Ready.
  EXPECT(loadingImage != nullptr);

  auto& rt = fixture.host().runtime();
  const auto primaryLoad =
      rt.global().getProperty(rt, "__imageLoadingPrimaryLoad");
  EXPECT(primaryLoad.isNumber() && primaryLoad.getNumber() >= 1.0);
  const auto primaryUri =
      rt.global().getProperty(rt, "__imageLoadingPrimaryUri");
  EXPECT(primaryUri.isString());

  const auto error = rt.global().getProperty(rt, "__imageLoadingError");
  EXPECT(!error.isNumber() || error.getNumber() == 0.0);
}

/// `Image.getSize` resolves through the `ImageLoader` TurboModule.
void metroStateUpdateCommitsThroughScheduler() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();
  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle((std::istreambuf_iterator<char>(bundleFile)),
                           std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));
  fixture.host().startSurface(kSurfaceId, "RNQnxHostStateUpdate", 640.0F, 480.0F);
  QElapsedTimer deadline;
  deadline.start();
  while (fixture.rootChildCount() == 0 && deadline.elapsed() < 1000) {
    fixture.host().pumpEvents();
  }
  EXPECT(fixture.rootChildCount() == 1);
  QQuickItem* view = fixture.root().childItems().at(0);
  EXPECT_NEAR(view->width(), 64.0);
  const int initialTransactions = fixture.host().delegate().mountedTransactionCount();
  EXPECT(fixture.host().evaluate("globalThis.__hostUpdateWidth()", "state-update.js"));
  deadline.restart();
  while (view->width() != 96.0 && deadline.elapsed() < 1000) {
    fixture.host().pumpEvents();
  }
  EXPECT_NEAR(view->width(), 96.0);
  EXPECT(fixture.host().delegate().mountedTransactionCount() > initialTransactions);
  EXPECT(fixture.host().delegate().lastFailures().empty());
}

void imageGetSize() {
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
      kSurfaceId, "RNQnxImageGetSize", 640.0F, 480.0F);
  for (int i = 0; i < 30; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());

  auto& rt = fixture.host().runtime();
  const auto result = rt.global().getProperty(rt, "__imageGetSize");
  EXPECT(result.isObject());
  const auto obj = result.getObject(rt);
  EXPECT(obj.getProperty(rt, "width").asNumber() == 1.0);
  EXPECT(obj.getProperty(rt, "height").asNumber() == 1.0);

  const auto error = rt.global().getProperty(rt, "__imageGetSizeError");
  EXPECT(error.isUndefined());
}

/// Exercise real Hermes Promise shapes and validation through the core adapter.
void imageLoaderPromiseContract() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();
  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle((std::istreambuf_iterator<char>(bundleFile)),
                           std::istreambuf_iterator<char>());
  EXPECT(fixture.host().evaluate(bundle, "rnqnx-playground.bundle.js"));
  EXPECT(fixture.host().evaluate(R"JS(
    globalThis.__imageContractDone = false;
    globalThis.__imageContractError = null;
    (async () => {
      const loader = nativeModuleProxy.ImageLoader;
      const uri = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
      const check = (ok, message) => { if (!ok) throw Error(message); };
      const rejects = async (promise) => {
        check(promise && typeof promise.then === 'function', 'not a Promise');
        let rejected = false;
        try { await promise; } catch (error) { rejected = error instanceof Error; }
        check(rejected, 'expected Error rejection');
      };
      check(await loader.prefetchImage(uri) === true, 'prefetch result');
      const size = await loader.getSize(uri);
      check(Array.isArray(size) && size[0] === 1 && size[1] === 1, 'size array');
      const headersSize = await loader.getSizeWithHeaders(uri, {});
      check(headersSize.width === 1 && headersSize.height === 1, 'headers size object');
      const cache = await loader.queryCache([uri, 'file:///definitely-missing-image']);
      check(cache[uri] === 'memory' && Object.keys(cache).length === 1, 'query cache');
      check(await loader.prefetchImageWithMetadata(uri, 'test-root', 1) === true, 'metadata prefetch');
      await rejects(loader.prefetchImage('file:///definitely-missing-image'));
      await rejects(loader.prefetchImage('data:image/png;base64,!!!!'));
      await rejects(loader.getSize(7));
      await rejects(loader.getSizeWithHeaders(uri, null));
      await rejects(loader.getSizeWithHeaders(uri, {'X-Key': 123}));
      await rejects(loader.queryCache([uri, 7]));
      await rejects(loader.queryCache(null));
      globalThis.__imageContractDone = true;
    })().catch(error => { globalThis.__imageContractError = String(error); });
  )JS", "image-contract.js"));
  auto& rt = fixture.host().runtime();
  QElapsedTimer deadline;
  deadline.start();
  while (deadline.elapsed() < 5000 &&
         !rt.global().getProperty(rt, "__imageContractDone").getBool() &&
         rt.global().getProperty(rt, "__imageContractError").isNull()) {
    rt.drainMicrotasks();
    fixture.host().pumpEvents();
  }
  EXPECT(rt.global().getProperty(rt, "__imageContractError").isNull());
  EXPECT(rt.global().getProperty(rt, "__imageContractDone").getBool());
}

/// `ImageBackground` mounts as a View with an Image and a child Text.
void imageBackgroundMounts() {
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
      kSurfaceId, "RNQnxImageBackground", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* root = fixture.root().childItems().at(0);
  EXPECT(root->childItems().size() == 2);
  QQuickItem* image = root->childItems().at(0);
  EXPECT(image->width() == 64.0);
  EXPECT(image->height() == 64.0);
  EXPECT(image->property("fillMode").toInt() == 2);  // Cover
}

/// `Image.resolveAssetSource` turns a bundler asset id into a URI.
void imageResolveAssetSource() {
  Fixture fixture;
  fixture.host().installTurboModuleBinding();
  fixture.host().installComponentProvider();
  fixture.host().installPlatformConstants();
  fixture.host().installTimers();

  std::ifstream bundleFile(RNQNX_PLAYGROUND_BUNDLE_PATH);
  const std::string bundle(
      (std::istreambuf_iterator<char>(bundleFile)),
      std::istreambuf_iterator<char>());
  const std::string sourceUrl =
      std::string("file://") + RNQNX_PLAYGROUND_BUNDLE_PATH;
  EXPECT(fixture.host().evaluate(bundle, sourceUrl));

  fixture.window().setWidth(640);
  fixture.window().setHeight(480);
  fixture.window().show();
  fixture.host().startSurface(
      kSurfaceId, "RNQnxImageResolveAsset", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());

  auto& rt = fixture.host().runtime();
  const auto resolved = rt.global().getProperty(rt, "__imageAssetResolved");
  EXPECT(resolved.isObject());
  const auto obj = resolved.getObject(rt);
  const auto uri = obj.getProperty(rt, "uri");
  EXPECT(uri.isString());
  EXPECT(uri.getString(rt).utf8(rt).find("qnx-asset.png") != std::string::npos);
}

/// `NativeAnimated` updates a view prop on the mounted Qt Quick item.
void nativeAnimatedOpacity() {
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
      kSurfaceId, "RNQnxNativeAnimated", 640.0F, 480.0F);
  for (int i = 0; i < 60; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* view = fixture.root().childItems().at(0);
  const double opacity = view->property("opacity").toDouble();
  EXPECT(std::abs(opacity - 0.5) < 0.05);
}

/// Modal mounts as a full-screen overlay with its child content.
void modalMounts() {
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
      kSurfaceId, "RNQnxModal", 640.0F, 480.0F);
  for (int i = 0; i < 20; ++i) {
    fixture.host().runtime().drainMicrotasks();
    fixture.host().pumpEvents();
  }

  EXPECT(fixture.host().delegate().lastFailures().empty());
  EXPECT(fixture.rootChildCount() == 1);

  QQuickItem* modal = fixture.root().childItems().at(0);
  EXPECT(modal->childItems().size() == 1);
  QQuickItem* content = modal->childItems().at(0);
  EXPECT(content->property("color").value<QColor>() ==
         QColor(QStringLiteral("#336699")));
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
      {"inputLifecycleOptOutPreservesExistingDispatch", inputLifecycleOptOutPreservesExistingDispatch},
      {"touchCancelEndsEveryContactAndAllowsIdReuse", touchCancelEndsEveryContactAndAllowsIdReuse},
      {"touchReleaseUsesTerminalCoordinatesAndKeepsOtherContact", touchReleaseUsesTerminalCoordinatesAndKeepsOtherContact},
      {"forgettingTouchTargetCancelsAndClearsResponder", forgettingTouchTargetCancelsAndClearsResponder},
      {"windowDeactivationCancelsTouchStream", windowDeactivationCancelsTouchStream},
      {"mouseCaptureCancellationDoesNotCreateHoverTouches", mouseCaptureCancellationDoesNotCreateHoverTouches},
      {"mouseReleaseClearsCaptureBeforeNativeListeners", mouseReleaseClearsCaptureBeforeNativeListeners},
      {"decoratedViewTargetsNearestRegisteredAncestor", decoratedViewTargetsNearestRegisteredAncestor},
#if RNQNX_ENABLE_FRAME_TRACE
      {"runtimeTraceFollowsOwnedHostBoundaries", runtimeTraceFollowsOwnedHostBoundaries},
#endif
      {"runtimeEvaluatesJavaScript", runtimeEvaluatesJavaScript},
      {"javaScriptErrorsAreReportedNotSwallowed",
       javaScriptErrorsAreReportedNotSwallowed},
      {"evaluateRejectsReentrantJavaScript", evaluateRejectsReentrantJavaScript},
      {"timerCallbacksRunMicrotaskCheckpoints", timerCallbacksRunMicrotaskCheckpoints},
      {"throwingMicrotasksAreReportedAndRecover", throwingMicrotasksAreReportedAndRecover},
      {"throwingTimerCallbacksAreReportedAndRecover", throwingTimerCallbacksAreReportedAndRecover},
      {"schedulerErrorsPreserveAndContainJavaScriptHandlers",
       schedulerErrorsPreserveAndContainJavaScriptHandlers},
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
      {"platformConstantsReportCurrentReactNativeVersion",
       platformConstantsReportCurrentReactNativeVersion},

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
      {"sectionListRenders", sectionListRenders},
      {"textInputMounts", textInputMounts},
      {"textInputPlaceholder", textInputPlaceholder},
      {"textInputMultiline", textInputMultiline},
      {"textEllipsis", textEllipsis},
      {"activityIndicatorMounts", activityIndicatorMounts},
      {"switchToggles", switchToggles},
      {"buttonMounts", buttonMounts},
      {"safeAreaViewMounts", safeAreaViewMounts},
      {"viewLayoutFires", viewLayoutFires},
      {"viewTransformApplies", viewTransformApplies},
      {"imageLoadEvents", imageLoadEvents},
      {"imageDefaultSourceFires", imageDefaultSourceFires},
      {"imageLoadingIndicator", imageLoadingIndicator},
      {"metroStateUpdateCommitsThroughScheduler", metroStateUpdateCommitsThroughScheduler},
      {"imageGetSize", imageGetSize},
      {"imageLoaderPromiseContract", imageLoaderPromiseContract},
      {"imageBackgroundMounts", imageBackgroundMounts},
      {"imageResolveAssetSource", imageResolveAssetSource},
      {"nativeAnimatedOpacity", nativeAnimatedOpacity},
      {"modalMounts", modalMounts},
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
