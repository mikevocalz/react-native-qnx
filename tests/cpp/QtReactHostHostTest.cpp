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
#include <QGuiApplication>
#include <QList>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>

#include <cstdlib>
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
