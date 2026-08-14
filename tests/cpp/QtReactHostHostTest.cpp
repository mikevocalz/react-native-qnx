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
#include <QGuiApplication>
#include <QList>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>

#include <cstdlib>
#include <fstream>
#include <exception>
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
        host_(adapter_),
        builder_(makeComponentBuilder(host_.contextContainer())) {
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  }

  ~Fixture() { host_.stopSurface(kSurfaceId); }

  QtReactHost& host() { return host_; }
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

  EXPECT(outer->childItems().size() == 1);
  QQuickItem* inner = outer->childItems().at(0);
  EXPECT_NEAR(inner->width(), 64.0);
  EXPECT_NEAR(inner->height(), 32.0);
  EXPECT(inner->property("color").value<QColor>() == QColor(0xff, 0x88, 0x00));
  EXPECT_NEAR(inner->opacity(), 0.5);
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
#endif
#endif
  };

  int failed = 0;
  for (const auto& test : tests) {
    try {
      test.run();
      std::cout << "[PASS] " << test.name << '\n';
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
