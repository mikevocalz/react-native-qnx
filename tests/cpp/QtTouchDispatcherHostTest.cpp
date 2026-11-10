/*
 * Exercises baseline Qt input selection through real QMouseEvents and the
 * production event filter. Only the expected React Native TouchEventEmitter is
 * registered, so a wrong hit cannot increment the dispatch count. Its event
 * dispatcher is deliberately absent: these are native routing assertions, not
 * Hermes/JavaScript delivery or target-device evidence.
 */

#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/QtTouchDispatcher.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using rnqnx::fabric::LayoutFrame;
using rnqnx::fabric::MountError;
using rnqnx::fabric::QtQuickMountingLayer;
using rnqnx::fabric::QtTouchDispatcher;
using rnqnx::fabric::Tag;
using rnqnx::views::QtQuickItemFactory;

constexpr Tag kRootTag = 1;
constexpr const char* kViewSource = "import QtQuick\nItem { visible: true }\n";
#if RNQNX_ENABLE_PLATFORM_SERVICES
constexpr bool kPlatformServices = true;
#else
constexpr bool kPlatformServices = false;
#endif

void expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void mount(MountError error) {
  if (error != MountError::None) {
    throw std::runtime_error(rnqnx::fabric::describe(error));
  }
}

class Fixture {
 public:
  explicit Fixture(bool inputLifecycle)
      : factory_(engine_),
        layer_(factory_, *window_.contentItem(), kRootTag),
        dispatcher_(layer_, *window_.contentItem(), inputLifecycle) {
    factory_.registerType(QStringLiteral("View"), QByteArray(kViewSource));
    window_.resize(320, 240);
    window_.show();
    QCoreApplication::processEvents();
    // Avoid QQuickWindow's additional mouse synthesis while retaining the
    // actual QObject event-filter entry point and window-space coordinates.
    inputSource_.installEventFilter(&dispatcher_);
  }

  QtQuickMountingLayer& layer() { return layer_; }
  QQuickItem& root() { return *window_.contentItem(); }

  QQuickItem* create(Tag tag, LayoutFrame frame) {
    mount(layer_.create(tag, QStringLiteral("View")));
    mount(layer_.updateLayout(tag, frame));
    return layer_.itemForTag(tag);
  }

  void listenOnly(Tag tag) {
    if (listeningTag_ >= 0) dispatcher_.setEventEmitter(listeningTag_, nullptr);
    dispatcher_.setEventEmitter(tag,
        std::make_shared<facebook::react::TouchEventEmitter>(
            nullptr, facebook::react::EventDispatcher::Weak{}));
    listeningTag_ = tag;
  }

  void click(QPointF point, const char* downMessage, const char* upMessage) {
    const int before = dispatcher_.dispatchedCount();
    QMouseEvent down(QEvent::MouseButtonPress, point, point,
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&inputSource_, &down);
    expect(dispatcher_.dispatchedCount() == before + 1, downMessage);
    QMouseEvent up(QEvent::MouseButtonRelease, point, point,
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&inputSource_, &up);
    expect(dispatcher_.dispatchedCount() == before + 2, upMessage);
  }

 private:
  QQmlEngine engine_;
  QQuickWindow window_;
  QtQuickItemFactory factory_;
  QtQuickMountingLayer layer_;
  QtTouchDispatcher dispatcher_;
  QObject inputSource_;
  Tag listeningTag_{-1};
};

void baselineUsesZOrderAndPreservesEqualZOrder() {
  for (const bool lifecycle : {false, true}) {
    Fixture fixture(lifecycle);
    auto* first = fixture.create(2, {0, 0, 320, 240});
    fixture.create(3, {0, 0, 320, 240});
    mount(fixture.layer().insert(kRootTag, 2, 0));
    mount(fixture.layer().insert(kRootTag, 3, 1));
    first->setZ(1);
    fixture.listenOnly(2);
    fixture.click({40, 40}, "higher-z view receives mouse down",
        "higher-z view receives mouse up");

    first->setZ(0);
    fixture.listenOnly(3);
    fixture.click({40, 40}, "equal-z last sibling receives mouse down",
        "equal-z last sibling receives mouse up");
    // A later creation inserted before existing Fabric siblings must not win
    // merely because it was created later.
    fixture.create(4, {0, 0, 320, 240});
    mount(fixture.layer().insert(kRootTag, 4, 0));
    fixture.click({40, 40}, "equal-z Fabric order receives mouse down",
        "equal-z Fabric order receives mouse up");
  }
}

#if RNQNX_ENABLE_PLATFORM_SERVICES
void addPortal(Fixture& fixture) {
  auto* declaringView = fixture.create(2, {10, 10, 50, 50});
  declaringView->setClip(true);
  fixture.create(3, {35, 45, 320, 240});
  fixture.create(4, {120, 80, 80, 60});
  mount(fixture.layer().setModalHost(3));
  // The adapter identifies the modal before its layout is applied, which
  // gives the portal viewport coordinates instead of the declaring offset.
  mount(fixture.layer().updateLayout(3, {35, 45, 320, 240}));
  mount(fixture.layer().insert(kRootTag, 2, 0));
  mount(fixture.layer().insert(2, 3, 0));
  mount(fixture.layer().insert(3, 4, 0));
  fixture.listenOnly(4);
}

void portalReceivesInputAfterResizeAndRecreation() {
  for (const bool lifecycle : {false, true}) {
    Fixture fixture(lifecycle);
    addPortal(fixture);
    fixture.click({160, 110}, "initial modal control receives mouse down",
        "initial modal control receives mouse up");
    QPointer<QQuickItem> overlay(fixture.layer().itemForTag(3)->parentItem());
    expect(overlay->size() == fixture.root().size(), "portal initially fills root");

    fixture.root().setWidth(800);
    fixture.root().setHeight(600);
    mount(fixture.layer().updateLayout(3, {0, 0, 800, 600}));
    mount(fixture.layer().updateLayout(4, {700, 500, 60, 40}));
    fixture.click({730, 520}, "resized modal control receives mouse down",
        "resized modal control receives mouse up");
    expect(overlay->size() == fixture.root().size(), "portal follows root growth");

    mount(fixture.layer().remove(2, 3, 0));
    expect(overlay.isNull(), "last modal removal destroys its portal");
    // Root notifications after deletion must not invoke a stale overlay/layer.
    fixture.root().setWidth(480);
    fixture.root().setHeight(360);
    mount(fixture.layer().updateLayout(3, {0, 0, 480, 360}));
    mount(fixture.layer().updateLayout(4, {390, 280, 60, 40}));
    mount(fixture.layer().insert(2, 3, 0));
    auto* replacement = fixture.layer().itemForTag(3)->parentItem();
    expect(replacement->size() == fixture.root().size(), "new portal uses current root size");
    fixture.click({420, 300}, "recreated modal control receives mouse down",
        "recreated modal control receives mouse up");
    fixture.root().setWidth(440);
    fixture.root().setHeight(330);
    expect(replacement->size() == fixture.root().size(), "new portal follows root shrink");
  }
}

void modalStaysAboveLaterBackgroundSibling() {
  for (const bool lifecycle : {false, true}) {
    Fixture fixture(lifecycle);
    addPortal(fixture);
    fixture.create(5, {0, 0, 320, 240});
    mount(fixture.layer().insert(kRootTag, 5, 1));
    auto* overlay = fixture.layer().itemForTag(3)->parentItem();
    expect(fixture.root().childItems().last() == fixture.layer().itemForTag(5),
        "background is appended after portal in childItems");
    expect(overlay->z() > fixture.layer().itemForTag(5)->z(), "portal is visually above background");
    fixture.click({160, 110}, "modal above later background receives mouse down",
        "modal above later background receives mouse up");
  }
}

void portalDisconnectsWhenLayerIsDestroyed() {
  QQmlEngine engine;
  QQuickWindow window;
  QtQuickItemFactory factory(engine);
  factory.registerType(QStringLiteral("View"), QByteArray(kViewSource));
  window.resize(320, 240);
  QPointer<QQuickItem> overlay;
  {
    QtQuickMountingLayer layer(factory, *window.contentItem(), kRootTag);
    mount(layer.create(2, QStringLiteral("View")));
    mount(layer.setModalHost(2));
    mount(layer.insert(kRootTag, 2, 0));
    overlay = layer.itemForTag(2)->parentItem();
  }
  expect(overlay.isNull(), "layer destruction destroys portal");
  window.contentItem()->setWidth(800);
  window.contentItem()->setHeight(600);
  QCoreApplication::processEvents();
}
#endif

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  const std::vector<std::pair<const char*, std::function<void()>>> tests{
      {"baselineUsesZOrderAndPreservesEqualZOrder", baselineUsesZOrderAndPreservesEqualZOrder},
#if RNQNX_ENABLE_PLATFORM_SERVICES
      {"portalReceivesInputAfterResizeAndRecreation", portalReceivesInputAfterResizeAndRecreation},
      {"modalStaysAboveLaterBackgroundSibling", modalStaysAboveLaterBackgroundSibling},
      {"portalDisconnectsWhenLayerIsDestroyed", portalDisconnectsWhenLayerIsDestroyed},
#endif
  };
  int failed = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
      ++failed;
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  }
  std::cout << tests.size() - failed << '/' << tests.size()
            << " baseline native routing cases passed; platform_services="
            << kPlatformServices << " desktop_input=0\n";
  return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
