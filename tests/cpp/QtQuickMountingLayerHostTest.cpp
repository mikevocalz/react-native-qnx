/*
 * Host tests for the Qt Quick half of the Fabric mounting layer. They run under
 * the offscreen QPA plugin against installed Qt on the macOS development host
 * (ADR-0010). They prove operation semantics, ordering, and ownership; they
 * prove nothing about QNX, the QNX QPA plugin, or the Pi 4.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

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

using rnqnx::fabric::LayoutFrame;
using rnqnx::fabric::MountError;
using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::Tag;
using rnqnx::views::QtQuickItemFactory;

constexpr Tag kRootTag = 1;
constexpr const char* kViewSource = "import QtQuick\nRectangle { visible: true }\n";
constexpr const char* kBrokenSource = "import QtQuick\nRectangle { nope: }\n";
constexpr const char* kNonItemSource = "import QtQml\nQtObject { }\n";

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

#define CHECK(expression)                        \
  do {                                           \
    if (!(expression)) {                         \
      fail(#expression, __FILE__, __LINE__);     \
    }                                            \
  } while (false)

#define CHECK_OK(expression)                                              \
  do {                                                                    \
    const MountError error_ = (expression);                               \
    if (error_ != MountError::None) {                                     \
      fail(#expression, __FILE__, __LINE__, rnqnx::fabric::describe(error_)); \
    }                                                                     \
  } while (false)

#define CHECK_ERROR(expression, expected)                                 \
  do {                                                                    \
    const MountError error_ = (expression);                               \
    if (error_ != (expected)) {                                           \
      fail(                                                               \
          #expression,                                                    \
          __FILE__,                                                       \
          __LINE__,                                                       \
          std::string("got ") + rnqnx::fabric::describe(error_));         \
    }                                                                     \
  } while (false)

/// Owns one engine, root window, factory, and layer per case so that no test
/// observes another test's tree.
class Fixture {
 public:
  Fixture() : factory_(engine_), layer_(factory_, *window_.contentItem(), kRootTag) {
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
  }

  QtQuickItemFactory& factory() { return factory_; }
  QtQuickMountingLayer& layer() { return layer_; }
  QQuickItem& root() { return *window_.contentItem(); }

  int rootChildCount() {
    return static_cast<int>(root().childItems().size());
  }

  MountError createView(Tag tag) {
    return layer_.create(tag, QStringLiteral("View"));
  }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
  QtQuickItemFactory factory_;
  QtQuickMountingLayer layer_;
};

void rootIsMountedButNotOwned() {
  Fixture fixture;
  CHECK(fixture.layer().mountedCount() == 1);
  CHECK(fixture.layer().itemForTag(kRootTag) == &fixture.root());
  CHECK_ERROR(fixture.layer().destroy(kRootTag), MountError::UnknownTag);
  CHECK(fixture.layer().itemForTag(kRootTag) == &fixture.root());
}

void createLeavesItemUnparented() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));

  QQuickItem* item = fixture.layer().itemForTag(2);
  CHECK(item != nullptr);
  CHECK(item->parentItem() == nullptr);
  CHECK(fixture.rootChildCount() == 0);
  CHECK(fixture.layer().mountedCount() == 2);
}

void insertParentsAndOrdersChildren() {
  Fixture fixture;
  for (Tag tag = 2; tag <= 4; ++tag) {
    CHECK_OK(fixture.createView(tag));
  }

  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));
  CHECK_OK(fixture.layer().insert(kRootTag, 3, 1));
  // Index 0 must land ahead of both existing children, not at the end.
  CHECK_OK(fixture.layer().insert(kRootTag, 4, 0));

  const QList<QQuickItem*> children = fixture.root().childItems();
  CHECK(children.size() == 3);
  CHECK(children.at(0) == fixture.layer().itemForTag(4));
  CHECK(children.at(1) == fixture.layer().itemForTag(2));
  CHECK(children.at(2) == fixture.layer().itemForTag(3));
}

void fabricIndicesExcludeQmlDecorations() {
  Fixture fixture;
  fixture.factory().registerType(QStringLiteral("DecoratedView"), QByteArray(
      "import QtQuick\nItem { Rectangle { objectName: 'background'; anchors.fill: parent } }\n"));
  CHECK_OK(fixture.layer().create(2, QStringLiteral("DecoratedView")));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));
  CHECK_OK(fixture.createView(3));
  CHECK_OK(fixture.createView(4));
  QQuickItem* parent = fixture.layer().itemForTag(2);
  QQuickItem* background = parent->childItems().at(0);
  CHECK_ERROR(fixture.layer().insert(2, 3, 1), MountError::IndexOutOfRange);
  CHECK_OK(fixture.layer().insert(2, 3, 0));
  CHECK_OK(fixture.layer().insert(2, 4, 0));
  CHECK(parent->childItems().size() == 3);
  CHECK(parent->childItems().at(0) == background);
  CHECK(parent->childItems().at(1) == fixture.layer().itemForTag(4));
  CHECK(parent->childItems().at(2) == fixture.layer().itemForTag(3));
  CHECK_ERROR(fixture.layer().remove(2, 3, 0), MountError::IndexOutOfRange);
  CHECK_OK(fixture.layer().remove(2, 3, 1));
  CHECK_OK(fixture.layer().remove(2, 4, 0));
  CHECK(parent->childItems().size() == 1);
  CHECK(parent->childItems().at(0) == background);
}

void insertRejectsBadArguments() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));

  CHECK_ERROR(fixture.layer().insert(99, 2, 0), MountError::UnknownParent);
  CHECK_ERROR(fixture.layer().insert(kRootTag, 99, 0), MountError::UnknownTag);
  CHECK_ERROR(fixture.layer().insert(kRootTag, 2, -1), MountError::IndexOutOfRange);
  CHECK_ERROR(fixture.layer().insert(kRootTag, 2, 1), MountError::IndexOutOfRange);
  CHECK(fixture.rootChildCount() == 0);
}

void removeDetachesWithoutDestroying() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));
  CHECK(fixture.rootChildCount() == 1);

  CHECK_OK(fixture.layer().remove(kRootTag, 2, 0));

  CHECK(fixture.rootChildCount() == 0);
  QQuickItem* item = fixture.layer().itemForTag(2);
  CHECK(item != nullptr);
  CHECK(item->parentItem() == nullptr);
  CHECK(fixture.layer().mountedCount() == 2);
}

void removeRejectsIndexDisagreement() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_OK(fixture.createView(3));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));
  CHECK_OK(fixture.layer().insert(kRootTag, 3, 1));

  CHECK_ERROR(fixture.layer().remove(kRootTag, 3, 0), MountError::IndexOutOfRange);
  CHECK_ERROR(fixture.layer().remove(2, 3, 0), MountError::NotAChildOfParent);
  CHECK(fixture.rootChildCount() == 2);

  CHECK_OK(fixture.layer().remove(kRootTag, 3, 1));
  CHECK(fixture.rootChildCount() == 1);
}

void destroyReleasesTagAndItem() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));
  CHECK_OK(fixture.layer().remove(kRootTag, 2, 0));

  CHECK_OK(fixture.layer().destroy(2));

  CHECK(fixture.layer().itemForTag(2) == nullptr);
  CHECK(fixture.layer().mountedCount() == 1);
  CHECK_ERROR(fixture.layer().destroy(2), MountError::UnknownTag);
}

void destroyDetachesStillMountedItem() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));

  // Out-of-order transaction: Delete without a preceding Remove.
  CHECK_OK(fixture.layer().destroy(2));

  CHECK(fixture.rootChildCount() == 0);
  CHECK(fixture.layer().itemForTag(2) == nullptr);
}

void nestedTreeIsTornDownWithTheLayer() {
  // ASan/UBSan turn any leak or use-after-free here into a failure.
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_OK(fixture.createView(3));
  CHECK_OK(fixture.createView(4));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));
  CHECK_OK(fixture.layer().insert(2, 3, 0));
  CHECK_OK(fixture.layer().insert(3, 4, 0));
  CHECK(fixture.layer().mountedCount() == 4);
}

void updateLayoutAppliesAbsoluteGeometry() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_OK(fixture.layer().insert(kRootTag, 2, 0));

  const LayoutFrame frame{.x = 12.5F, .y = 24.25F, .width = 100.0F, .height = 48.5F};
  CHECK_OK(fixture.layer().updateLayout(2, frame));

  QQuickItem* item = fixture.layer().itemForTag(2);
  CHECK(item->x() == static_cast<qreal>(frame.x));
  CHECK(item->y() == static_cast<qreal>(frame.y));
  CHECK(item->width() == static_cast<qreal>(frame.width));
  CHECK(item->height() == static_cast<qreal>(frame.height));

  CHECK_ERROR(fixture.layer().updateLayout(99, frame), MountError::UnknownTag);
}

void unregisteredComponentTypeFails() {
  Fixture fixture;
  CHECK_ERROR(
      fixture.layer().create(2, QStringLiteral("Text")),
      MountError::UnknownComponentType);
  CHECK(fixture.layer().mountedCount() == 1);
  CHECK(!fixture.layer().lastError().isEmpty());
}

void brokenQmlSourceFails() {
  Fixture fixture;
  fixture.factory().registerType(
      QStringLiteral("Broken"), QByteArray(kBrokenSource));

  CHECK_ERROR(
      fixture.layer().create(2, QStringLiteral("Broken")),
      MountError::UnknownComponentType);
  CHECK(!fixture.factory().isCompiled(QStringLiteral("Broken")));
  CHECK(fixture.layer().mountedCount() == 1);
}

void nonItemTypeFails() {
  Fixture fixture;
  fixture.factory().registerType(
      QStringLiteral("NotAnItem"), QByteArray(kNonItemSource));

  CHECK_ERROR(
      fixture.layer().create(2, QStringLiteral("NotAnItem")),
      MountError::ItemCreationFailed);
  CHECK(fixture.layer().mountedCount() == 1);
}

void duplicateTagFails() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK_ERROR(fixture.createView(2), MountError::DuplicateTag);
  CHECK(fixture.layer().mountedCount() == 2);
}

void reregisteringATypeReplacesIt() {
  Fixture fixture;
  CHECK_OK(fixture.createView(2));
  CHECK(fixture.factory().isCompiled(QStringLiteral("View")));

  fixture.factory().registerType(
      QStringLiteral("View"), QByteArray(kBrokenSource));
  CHECK(!fixture.factory().isCompiled(QStringLiteral("View")));
  CHECK_ERROR(fixture.createView(3), MountError::UnknownComponentType);
}

struct TestCase {
  const char* name;
  std::function<void()> run;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);

  const std::vector<TestCase> tests{
      {"rootIsMountedButNotOwned", rootIsMountedButNotOwned},
      {"createLeavesItemUnparented", createLeavesItemUnparented},
      {"insertParentsAndOrdersChildren", insertParentsAndOrdersChildren},
      {"insertRejectsBadArguments", insertRejectsBadArguments},
      {"fabricIndicesExcludeQmlDecorations", fabricIndicesExcludeQmlDecorations},
      {"removeDetachesWithoutDestroying", removeDetachesWithoutDestroying},
      {"removeRejectsIndexDisagreement", removeRejectsIndexDisagreement},
      {"destroyReleasesTagAndItem", destroyReleasesTagAndItem},
      {"destroyDetachesStillMountedItem", destroyDetachesStillMountedItem},
      {"nestedTreeIsTornDownWithTheLayer", nestedTreeIsTornDownWithTheLayer},
      {"updateLayoutAppliesAbsoluteGeometry", updateLayoutAppliesAbsoluteGeometry},
      {"unregisteredComponentTypeFails", unregisteredComponentTypeFails},
      {"brokenQmlSourceFails", brokenQmlSourceFails},
      {"nonItemTypeFails", nonItemTypeFails},
      {"duplicateTagFails", duplicateTagFails},
      {"reregisteringATypeReplacesIt", reregisteringATypeReplacesIt},
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
    std::cerr << failed << " QtQuickMountingLayer host test(s) failed.\n";
    return EXIT_FAILURE;
  }

  std::cout << "All " << tests.size()
            << " QtQuickMountingLayer host tests passed.\n";
  return EXIT_SUCCESS;
}
