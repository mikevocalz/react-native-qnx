/*
 * End-to-end host test: a real facebook::react::ShadowTree is committed, React
 * Native's own differ produces the mutations, MountingCoordinator hands them
 * over as a MountingTransaction, and the adapter applies them to Qt Quick items.
 * The geometry asserted here is Yoga's, computed by React Native, not by this
 * repository.
 *
 * Host evidence only. Nothing is cross-compiled for QNX, nothing runs on the
 * Pi 4, and no JavaScript participates: the tree is committed from C++.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>
#include <react/renderer/components/view/ViewShadowNode.h>
#include <react/renderer/element/ComponentBuilder.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/element/Element.h>
#include <react/renderer/mounting/MountingCoordinator.h>
#include <react/renderer/mounting/MountingTransaction.h>
#include <react/renderer/mounting/ShadowTree.h>
#include <react/renderer/mounting/ShadowTreeDelegate.h>
#include <react/utils/ContextContainer.h>

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
using facebook::react::LayoutConstraints;
using facebook::react::LayoutContext;
using facebook::react::MountingCoordinator;
using facebook::react::RootShadowNode;
using facebook::react::ShadowNode;

/// React Native 0.86.2 spells this type inline rather than aliasing it on
/// ShadowNode (ShadowNodeFragment.h:27).
using ShadowNodeList = std::vector<std::shared_ptr<const ShadowNode>>;
using facebook::react::ShadowNodeFragment;
using facebook::react::ShadowTree;
using facebook::react::ShadowTreeDelegate;
using facebook::react::SurfaceId;
using facebook::react::ViewShadowNode;
using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::ShadowViewMutationAdapter;
using rnqnx::fabric::Tag;
using rnqnx::views::QtQuickItemFactory;

constexpr SurfaceId kSurfaceId = 1;
constexpr Tag kRootTag = 1;
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

#define EXPECT_NEAR(actual, expected)                                     \
  do {                                                                    \
    const double actual_ = static_cast<double>(actual);                   \
    const double expected_ = static_cast<double>(expected);               \
    if (actual_ < expected_ - 0.01 || actual_ > expected_ + 0.01) {       \
      std::ostringstream detail_;                                         \
      detail_ << actual_ << " != " << expected_;                          \
      fail(#actual " ~= " #expected, __FILE__, __LINE__, detail_.str());  \
    }                                                                     \
  } while (false)

/// React Native requires a delegate on every shadow tree. Nothing here needs to
/// intercept or reschedule a commit, so each hook is an explicit pass-through
/// rather than a stub that pretends to do work.
class PassThroughShadowTreeDelegate final : public ShadowTreeDelegate {
 public:
  RootShadowNode::Unshared shadowTreeWillCommit(
      const ShadowTree& /*shadowTree*/,
      const RootShadowNode::Shared& /*oldRootShadowNode*/,
      const RootShadowNode::Unshared& newRootShadowNode,
      const facebook::react::ShadowTreeCommitOptions& /*commitOptions*/)
      const override {
    return newRootShadowNode;
  }

  void shadowTreeDidFinishTransaction(
      std::shared_ptr<const MountingCoordinator> /*mountingCoordinator*/,
      bool /*mountSynchronously*/) const override {}

  void shadowTreeDidFinishReactCommit(
      const ShadowTree& /*shadowTree*/) const override {}

  void shadowTreeDidPromoteReactRevision(
      const ShadowTree& /*shadowTree*/) const override {}
};

/// Yoga style helper.
///
/// `collapsable` is turned off deliberately. React Native flattens views that
/// carry no visual or interactive props, and a flattened view produces no mount
/// mutation at all — its layout is folded into its parent. That behaviour is
/// real and the platform must handle it, but a layout test that relied on it
/// would be asserting nothing, so these views opt out.
std::shared_ptr<const ViewShadowNode::ConcreteProps> viewProps(
    const std::function<void(ViewShadowNode::ConcreteProps&)>& configure) {
  auto props = std::make_shared<ViewShadowNode::ConcreteProps>();
  props->collapsable = false;
  configure(*props);
  return props;
}

/// React Native's own simpleComponentBuilder (react/renderer/element/testUtils.h)
/// registers modal and text descriptors, which include codegen output
/// (FBReactNativeSpec) that this repository does not generate yet. Registering
/// only the descriptors under test keeps the tree real without pulling in
/// artifacts we cannot honestly claim to build.
ComponentBuilder makeViewOnlyComponentBuilder(
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
        layer_(factory_, *window_.contentItem(), kRootTag),
        adapter_(layer_),
        contextContainer_(std::make_shared<const ContextContainer>()),
        builder_(makeViewOnlyComponentBuilder(contextContainer_)),
        shadowTree_(
            kSurfaceId,
            layoutConstraints(),
            LayoutContext{},
            delegate_,
            *contextContainer_) {
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  }

  static LayoutConstraints layoutConstraints() {
    LayoutConstraints constraints;
    constraints.minimumSize = {.width = 0, .height = 0};
    constraints.maximumSize = {.width = 1280, .height = 720};
    return constraints;
  }

  /// Commits \a children as the root's children and applies the resulting
  /// transaction. Returns the number of mutations React Native produced.
  std::size_t commitAndMount(const ShadowNodeList& children) {
    shadowTree_.commit(
        [&](const RootShadowNode& oldRootShadowNode) {
          return std::static_pointer_cast<RootShadowNode>(
              oldRootShadowNode.ShadowNode::clone(ShadowNodeFragment{
                  .props = ShadowNodeFragment::propsPlaceholder(),
                  .children = std::make_shared<const ShadowNodeList>(
                      children),
              }));
        },
        {});

    auto transaction = shadowTree_.getMountingCoordinator()->pullTransaction();
    if (!transaction.has_value()) {
      return 0;
    }

    const auto& mutations = transaction->getMutations();
    const auto failures = adapter_.apply(mutations);
    if (!failures.empty()) {
      throw TestFailure(
          "adapter reported a failure: " +
          failures.front().detail.toStdString());
    }
    return mutations.size();
  }

  ComponentBuilder& builder() { return builder_; }
  QtQuickMountingLayer& layer() { return layer_; }
  QQuickItem& root() { return *window_.contentItem(); }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
  QtQuickItemFactory factory_;
  QtQuickMountingLayer layer_;
  ShadowViewMutationAdapter adapter_;
  std::shared_ptr<const ContextContainer> contextContainer_;
  ComponentBuilder builder_;
  PassThroughShadowTreeDelegate delegate_;
  ShadowTree shadowTree_;
};

void fixedSizeViewGetsYogaGeometry() {
  Fixture fixture;

  auto element = Element<ViewShadowNode>().tag(2).props(viewProps(
      [](ViewShadowNode::ConcreteProps& props) {
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Width,
            facebook::yoga::StyleSizeLength::points(120.0F));
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Height,
            facebook::yoga::StyleSizeLength::points(60.0F));
      }));

  const std::size_t mutationCount =
      fixture.commitAndMount({fixture.builder().build(element)});
  EXPECT(mutationCount > 0);

  QQuickItem* item = fixture.layer().itemForTag(2);
  EXPECT(item != nullptr);
  EXPECT(item->parentItem() == &fixture.root());
  EXPECT_NEAR(item->width(), 120.0);
  EXPECT_NEAR(item->height(), 60.0);
}

void flexChildrenSplitTheRootWidth() {
  Fixture fixture;

  const auto flexChild = [](Tag tag, float flex) {
    return Element<ViewShadowNode>().tag(tag).props(
        viewProps([flex](ViewShadowNode::ConcreteProps& props) {
          props.yogaStyle.setFlex(facebook::yoga::FloatOptional{flex});
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Height,
              facebook::yoga::StyleSizeLength::points(40.0F));
        }));
  };

  // The root is 1280 wide and lays out in a row only if told to; the default
  // flex direction is column, so flex here divides the height. Width therefore
  // stretches to the root and height splits 1:3.
  auto row = Element<ViewShadowNode>()
                 .tag(2)
                 .props(viewProps([](ViewShadowNode::ConcreteProps& props) {
                   props.yogaStyle.setDimension(
                       facebook::yoga::Dimension::Width,
                       facebook::yoga::StyleSizeLength::points(400.0F));
                   props.yogaStyle.setDimension(
                       facebook::yoga::Dimension::Height,
                       facebook::yoga::StyleSizeLength::points(160.0F));
                 }))
                 .children({flexChild(3, 1.0F), flexChild(4, 3.0F)});

  fixture.commitAndMount({fixture.builder().build(row)});

  QQuickItem* parent = fixture.layer().itemForTag(2);
  QQuickItem* first = fixture.layer().itemForTag(3);
  QQuickItem* second = fixture.layer().itemForTag(4);
  EXPECT(parent != nullptr);
  EXPECT(first != nullptr);
  EXPECT(second != nullptr);

  EXPECT(first->parentItem() == parent);
  EXPECT(second->parentItem() == parent);

  // 160 split 1:3 by flex grow, and children stack down the column.
  EXPECT_NEAR(first->height(), 40.0);
  EXPECT_NEAR(second->height(), 120.0);
  EXPECT_NEAR(first->y(), 0.0);
  EXPECT_NEAR(second->y(), 40.0);
  EXPECT_NEAR(first->width(), 400.0);

  // Yoga positions children relative to the parent, and Qt items are positioned
  // in their parent's coordinate space, so the values transfer unchanged.
  EXPECT_NEAR(parent->x(), 0.0);
  EXPECT_NEAR(parent->y(), 0.0);
}

void paddingOffsetsChildOrigin() {
  Fixture fixture;

  auto child = Element<ViewShadowNode>().tag(3).props(
      viewProps([](ViewShadowNode::ConcreteProps& props) {
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Width,
            facebook::yoga::StyleSizeLength::points(50.0F));
        props.yogaStyle.setDimension(
            facebook::yoga::Dimension::Height,
            facebook::yoga::StyleSizeLength::points(50.0F));
      }));

  auto parent = Element<ViewShadowNode>()
                    .tag(2)
                    .props(viewProps([](ViewShadowNode::ConcreteProps& props) {
                      props.yogaStyle.setDimension(
                          facebook::yoga::Dimension::Width,
                          facebook::yoga::StyleSizeLength::points(200.0F));
                      props.yogaStyle.setDimension(
                          facebook::yoga::Dimension::Height,
                          facebook::yoga::StyleSizeLength::points(200.0F));
                      props.yogaStyle.setPadding(
                          facebook::yoga::Edge::All,
                          facebook::yoga::StyleLength::points(16.0F));
                    }))
                    .children({child});

  fixture.commitAndMount({fixture.builder().build(parent)});

  QQuickItem* childItem = fixture.layer().itemForTag(3);
  EXPECT(childItem != nullptr);
  EXPECT_NEAR(childItem->x(), 16.0);
  EXPECT_NEAR(childItem->y(), 16.0);
  EXPECT_NEAR(childItem->width(), 50.0);
}

void secondCommitUpdatesGeometryInPlace() {
  Fixture fixture;

  const auto sized = [](float width) {
    return Element<ViewShadowNode>().tag(2).props(
        viewProps([width](ViewShadowNode::ConcreteProps& props) {
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Width,
              facebook::yoga::StyleSizeLength::points(width));
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Height,
              facebook::yoga::StyleSizeLength::points(30.0F));
        }));
  };

  fixture.commitAndMount({fixture.builder().build(sized(100.0F))});
  QQuickItem* item = fixture.layer().itemForTag(2);
  EXPECT(item != nullptr);
  EXPECT_NEAR(item->width(), 100.0);

  fixture.commitAndMount({fixture.builder().build(sized(250.0F))});

  // The same item is resized rather than replaced, which is what React Native's
  // differ emits for a props-only change.
  EXPECT(fixture.layer().itemForTag(2) == item);
  EXPECT_NEAR(item->width(), 250.0);
  EXPECT_NEAR(item->height(), 30.0);
}

void removingAChildUnmountsIt() {
  Fixture fixture;

  const auto leaf = [](Tag tag) {
    return Element<ViewShadowNode>().tag(tag).props(
        viewProps([](ViewShadowNode::ConcreteProps& props) {
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Width,
              facebook::yoga::StyleSizeLength::points(20.0F));
          props.yogaStyle.setDimension(
              facebook::yoga::Dimension::Height,
              facebook::yoga::StyleSizeLength::points(20.0F));
        }));
  };

  fixture.commitAndMount(
      {fixture.builder().build(leaf(2)), fixture.builder().build(leaf(3))});
  EXPECT(fixture.root().childItems().size() == 2);

  fixture.commitAndMount({fixture.builder().build(leaf(2))});

  EXPECT(fixture.root().childItems().size() == 1);
  EXPECT(fixture.layer().itemForTag(2) != nullptr);
  EXPECT(fixture.layer().itemForTag(3) == nullptr);
}

struct TestCase {
  const char* name;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);

  const std::vector<TestCase> tests{
      {"fixedSizeViewGetsYogaGeometry", fixedSizeViewGetsYogaGeometry},
      {"flexChildrenSplitTheRootWidth", flexChildrenSplitTheRootWidth},
      {"paddingOffsetsChildOrigin", paddingOffsetsChildOrigin},
      {"secondCommitUpdatesGeometryInPlace", secondCommitUpdatesGeometryInPlace},
      {"removingAChildUnmountsIt", removingAChildUnmountsIt},
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
    std::cerr << failed << " ShadowTree mounting host test(s) failed.\n";
    return EXIT_FAILURE;
  }

  std::cout << "All " << tests.size()
            << " ShadowTree mounting host tests passed.\n";
  return EXIT_SUCCESS;
}
