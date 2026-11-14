/*
 * Drives the Qt Quick mounting layer with real facebook::react::
 * ShadowViewMutation values built through React Native's own static factories,
 * compiled against the installed React Native 0.86.2 headers.
 *
 * Host evidence only: the transactions are constructed here rather than pulled
 * from a running surface, and nothing has been cross-compiled for QNX.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <react/renderer/mounting/ShadowView.h>
#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/graphics/Color.h>
#include <QColor>
#include <react/renderer/mounting/ShadowViewMutation.h>

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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using facebook::react::ShadowView;
using facebook::react::ShadowViewMutation;
using rnqnx::fabric::MountError;
using rnqnx::fabric::MutationFailure;
using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::ShadowViewMutationAdapter;
using rnqnx::fabric::Tag;
using rnqnx::views::QtQuickItemFactory;

constexpr Tag kRootTag = 1;
constexpr const char* kViewSource = "import QtQuick\nRectangle { id: root; visible: true; property alias backgroundColor: root.color }\n";

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

#define EXPECT(expression)                    \
  do {                                       \
    if (!(expression)) {                     \
      fail(#expression, __FILE__, __LINE__); \
    }                                        \
  } while (false)

#define EXPECT_NO_FAILURES(failures)                                   \
  do {                                                                \
    if (!(failures).empty()) {                                        \
      fail(                                                           \
          #failures " is empty",                                      \
          __FILE__,                                                   \
          __LINE__,                                                   \
          (failures).front().detail.toStdString());                   \
    }                                                                 \
  } while (false)

ShadowView makeShadowView(
    Tag tag,
    const char* componentName,
    float x = 0.0F,
    float y = 0.0F,
    float width = 0.0F,
    float height = 0.0F) {
  ShadowView view;
  view.tag = tag;
  view.componentName = componentName;
  view.layoutMetrics.frame.origin.x = x;
  view.layoutMetrics.frame.origin.y = y;
  view.layoutMetrics.frame.size.width = width;
  view.layoutMetrics.frame.size.height = height;
  return view;
}

class Fixture {
 public:
  Fixture()
      : factory_(engine_),
        layer_(factory_, *window_.contentItem(), kRootTag),
        adapter_(layer_) {
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
  }

  ShadowViewMutationAdapter& adapter() { return adapter_; }
  QtQuickMountingLayer& layer() { return layer_; }
  QQuickItem& root() { return *window_.contentItem(); }

  int rootChildCount() { return static_cast<int>(root().childItems().size()); }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
  QtQuickItemFactory factory_;
  QtQuickMountingLayer layer_;
  ShadowViewMutationAdapter adapter_;
};

void createInsertMountsAndPositions() {
  Fixture fixture;
  const ShadowView view = makeShadowView(2, "View", 5.0F, 6.0F, 70.0F, 80.0F);

  const ShadowViewMutation::List mutations{
      ShadowViewMutation::CreateMutation(view),
      ShadowViewMutation::InsertMutation(kRootTag, view, 0),
  };

  EXPECT_NO_FAILURES(fixture.adapter().apply(mutations));

  QQuickItem* item = fixture.layer().itemForTag(2);
  EXPECT(item != nullptr);
  EXPECT(item->parentItem() == &fixture.root());
  // Create carries the first layout, so geometry is correct before any Update.
  EXPECT(item->x() == 5.0);
  EXPECT(item->y() == 6.0);
  EXPECT(item->width() == 70.0);
  EXPECT(item->height() == 80.0);
}

void insertOrderFollowsMutationIndex() {
  Fixture fixture;
  const ShadowView first = makeShadowView(2, "View");
  const ShadowView second = makeShadowView(3, "View");
  const ShadowView third = makeShadowView(4, "View");

  const ShadowViewMutation::List mutations{
      ShadowViewMutation::CreateMutation(first),
      ShadowViewMutation::CreateMutation(second),
      ShadowViewMutation::CreateMutation(third),
      ShadowViewMutation::InsertMutation(kRootTag, first, 0),
      ShadowViewMutation::InsertMutation(kRootTag, second, 1),
      ShadowViewMutation::InsertMutation(kRootTag, third, 1),
  };

  EXPECT_NO_FAILURES(fixture.adapter().apply(mutations));

  const QList<QQuickItem*> children = fixture.root().childItems();
  EXPECT(children.size() == 3);
  EXPECT(children.at(0) == fixture.layer().itemForTag(2));
  EXPECT(children.at(1) == fixture.layer().itemForTag(4));
  EXPECT(children.at(2) == fixture.layer().itemForTag(3));
}

void updateAppliesNewFrame() {
  Fixture fixture;
  const ShadowView before = makeShadowView(2, "View", 0.0F, 0.0F, 10.0F, 10.0F);
  const ShadowView after =
      makeShadowView(2, "View", 100.0F, 200.0F, 300.0F, 400.0F);

  EXPECT_NO_FAILURES(fixture.adapter().apply(ShadowViewMutation::List{
      ShadowViewMutation::CreateMutation(before),
      ShadowViewMutation::InsertMutation(kRootTag, before, 0),
  }));
  EXPECT_NO_FAILURES(fixture.adapter().apply(ShadowViewMutation::List{
      ShadowViewMutation::UpdateMutation(before, after, kRootTag),
  }));

  QQuickItem* item = fixture.layer().itemForTag(2);
  EXPECT(item->x() == 100.0);
  EXPECT(item->y() == 200.0);
  EXPECT(item->width() == 300.0);
  EXPECT(item->height() == 400.0);
}

void transparentAndRemovedBackgroundClearThePreviousFill() {
  Fixture fixture;
  auto selected = makeShadowView(2, "View", 0, 0, 100, 40);
  auto props = std::make_shared<facebook::react::ViewProps>();
  props->backgroundColor = facebook::react::colorFromRGBA(27, 83, 94, 255);
  selected.props = props;
  EXPECT_NO_FAILURES(fixture.adapter().apply({
      ShadowViewMutation::CreateMutation(selected),
      ShadowViewMutation::InsertMutation(kRootTag, selected, 0)}));
  auto* item = fixture.layer().itemForTag(2);
  EXPECT(item->property("backgroundColor").value<QColor>().alpha() == 255);
  for (bool explicitTransparent : {true, false}) {
    auto clear = selected;
    auto clearedProps = std::make_shared<facebook::react::ViewProps>();
    if (explicitTransparent) clearedProps->backgroundColor = facebook::react::clearColor();
    clear.props = clearedProps;
    EXPECT_NO_FAILURES(fixture.adapter().apply({ShadowViewMutation::UpdateMutation(selected, clear, kRootTag)}));
    EXPECT(item->property("backgroundColor").value<QColor>().alpha() == 0);
    EXPECT_NO_FAILURES(fixture.adapter().apply({ShadowViewMutation::UpdateMutation(clear, selected, kRootTag)}));
    EXPECT(item->property("backgroundColor").value<QColor>().alpha() == 255);
  }
}

void removeThenDeleteUnmountsAndFrees() {
  Fixture fixture;
  const ShadowView view = makeShadowView(2, "View");

  EXPECT_NO_FAILURES(fixture.adapter().apply(ShadowViewMutation::List{
      ShadowViewMutation::CreateMutation(view),
      ShadowViewMutation::InsertMutation(kRootTag, view, 0),
  }));
  EXPECT(fixture.rootChildCount() == 1);

  EXPECT_NO_FAILURES(fixture.adapter().apply(ShadowViewMutation::List{
      ShadowViewMutation::RemoveMutation(kRootTag, view, 0),
      ShadowViewMutation::DeleteMutation(view),
  }));

  EXPECT(fixture.rootChildCount() == 0);
  EXPECT(fixture.layer().itemForTag(2) == nullptr);
  EXPECT(fixture.layer().mountedCount() == 1);
}

void nestedTransactionBuildsATree() {
  Fixture fixture;
  const ShadowView parent = makeShadowView(2, "View", 0.0F, 0.0F, 200.0F, 100.0F);
  const ShadowView child = makeShadowView(3, "View", 4.0F, 8.0F, 20.0F, 10.0F);

  EXPECT_NO_FAILURES(fixture.adapter().apply(ShadowViewMutation::List{
      ShadowViewMutation::CreateMutation(parent),
      ShadowViewMutation::CreateMutation(child),
      ShadowViewMutation::InsertMutation(kRootTag, parent, 0),
      ShadowViewMutation::InsertMutation(2, child, 0),
  }));

  QQuickItem* parentItem = fixture.layer().itemForTag(2);
  QQuickItem* childItem = fixture.layer().itemForTag(3);
  EXPECT(childItem->parentItem() == parentItem);
  EXPECT(childItem->x() == 4.0);
  EXPECT(fixture.rootChildCount() == 1);
}

void unmappedComponentIsReportedNotSkipped() {
  Fixture fixture;
  const ShadowView unmapped = makeShadowView(2, "Text");
  const ShadowView mapped = makeShadowView(3, "View");

  const std::vector<MutationFailure> failures =
      fixture.adapter().apply(ShadowViewMutation::List{
          ShadowViewMutation::CreateMutation(unmapped),
          ShadowViewMutation::CreateMutation(mapped),
          ShadowViewMutation::InsertMutation(kRootTag, mapped, 0),
      });

  EXPECT(failures.size() == 1);
  EXPECT(failures.front().error == MountError::UnknownComponentType);
  EXPECT(failures.front().tag == 2);
  EXPECT(failures.front().detail.contains(QStringLiteral("Text")));

  // The rest of the transaction still applied.
  EXPECT(fixture.layer().itemForTag(2) == nullptr);
  EXPECT(fixture.layer().itemForTag(3) != nullptr);
  EXPECT(fixture.rootChildCount() == 1);
}

void insertIntoMissingParentIsReported() {
  Fixture fixture;
  const ShadowView view = makeShadowView(2, "View");

  const std::vector<MutationFailure> failures =
      fixture.adapter().apply(ShadowViewMutation::List{
          ShadowViewMutation::CreateMutation(view),
          ShadowViewMutation::InsertMutation(99, view, 0),
      });

  EXPECT(failures.size() == 1);
  EXPECT(failures.front().error == MountError::UnknownParent);
  EXPECT(failures.front().tag == 2);
  EXPECT(fixture.rootChildCount() == 0);
}

struct TestCase {
  const char* name;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);

  const std::vector<TestCase> tests{
      {"createInsertMountsAndPositions", createInsertMountsAndPositions},
      {"insertOrderFollowsMutationIndex", insertOrderFollowsMutationIndex},
      {"updateAppliesNewFrame", updateAppliesNewFrame},
      {"transparentAndRemovedBackgroundClearThePreviousFill", transparentAndRemovedBackgroundClearThePreviousFill},
      {"removeThenDeleteUnmountsAndFrees", removeThenDeleteUnmountsAndFrees},
      {"nestedTransactionBuildsATree", nestedTransactionBuildsATree},
      {"unmappedComponentIsReportedNotSkipped", unmappedComponentIsReportedNotSkipped},
      {"insertIntoMissingParentIsReported", insertIntoMissingParentIsReported},
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
    std::cerr << failed << " ShadowViewMutationAdapter host test(s) failed.\n";
    return EXIT_FAILURE;
  }

  std::cout << "All " << tests.size()
            << " ShadowViewMutationAdapter host tests passed.\n";
  return EXIT_SUCCESS;
}
