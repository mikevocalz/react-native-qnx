#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#if RNQNX_ENABLE_PLATFORM_SERVICES
#include "react-native-qnx/runtime/QtAccessibilityInfoModule.h"
#include "react-native-qnx/runtime/QtDeviceEventManagerModule.h"
#include <ReactCommon/CallInvoker.h>
#endif

#include <QAccessible>
#include <QAccessibilityHints>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStyleHints>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace jsi = facebook::jsi;
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }

struct Fixture {
  QQmlEngine engine;
  QQuickWindow window;
  rnqnx::views::QtQuickItemFactory factory{engine};
  rnqnx::fabric::QtQuickMountingLayer layer{factory, *window.contentItem(), 11};
  rnqnx::fabric::ShadowViewMutationAdapter adapter{layer};
  std::unique_ptr<rnqnx::runtime::QtReactHost> host{std::make_unique<rnqnx::runtime::QtReactHost>(adapter)};

  explicit Fixture(bool installBinding = true) {
    window.resize(320, 240);
    factory.registerType(QStringLiteral("View"), QByteArray("import QtQuick\nItem { property color backgroundColor: 'transparent' }"));
    adapter.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    adapter.mapComponent(QStringLiteral("ModalHostView"), QStringLiteral("View"));
    if (installBinding) host->installTurboModuleBinding();
    host->installComponentProvider();
    host->installPlatformConstants();
    host->installTimers();
    window.show();
  }
  void eval(const std::string& source) {
    require(host->evaluate(source, "platform-services-assert.js"), host->lastError().toStdString());
  }
  void check(const std::string& expression) {
    eval("if (!(" + expression + ")) throw new Error('Failed: " + expression + "');");
  }
  void pump() {
    host->runtime().drainMicrotasks();
    host->pumpEvents();
    require(host->lastError().isEmpty(), host->lastError().toStdString());
    require(host->javaScriptErrors().empty(), "React reported a JavaScript error");
    require(host->delegate().lastFailures().empty(), "Fabric mount failed");
  }
  template <typename Predicate> void until(Predicate predicate, const char* message) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 3000) pump();
    require(predicate(), message);
  }
};

[[maybe_unused]] void key(QQuickWindow& window, int code, bool repeat = false, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent press(QEvent::KeyPress, code, modifiers, QString(), repeat);
  press.setTimestamp(1234);
  QCoreApplication::sendEvent(&window, &press);
  QKeyEvent release(QEvent::KeyRelease, code, modifiers, QString(), repeat);
  release.setTimestamp(1250);
  QCoreApplication::sendEvent(&window, &release);
}

#if RNQNX_ENABLE_PLATFORM_SERVICES
struct ObservedEvents {
  int announcements{0};
  int focus{0};
  QString message;
  QAccessible::AnnouncementPoliteness politeness{QAccessible::AnnouncementPoliteness::Polite};
};
ObservedEvents* observations{nullptr};
void observe(QAccessibleEvent* event) {
  if (!observations) return;
  if (event->type() == QAccessible::Announcement) {
    auto* announcement = static_cast<QAccessibleAnnouncementEvent*>(event);
    ++observations->announcements;
    observations->message = announcement->message();
    observations->politeness = announcement->politeness();
  }
  if (event->type() == QAccessible::Focus) ++observations->focus;
}

class DeferredInvoker final : public facebook::react::CallInvoker {
 public:
  void invokeAsync(facebook::react::CallFunc&& task) noexcept override { tasks.push_back(std::move(task)); }
  void invokeSync(facebook::react::CallFunc&&) override { throw std::logic_error("No synchronous cross-context calls"); }
  void drain(jsi::Runtime& runtime) {
    auto pending = std::move(tasks); tasks.clear();
    for (auto& task : pending) task(runtime);
  }
  std::vector<facebook::react::CallFunc> tasks;
};

void lifetime() {
  Fixture fixture(false);
  fixture.eval("globalThis.late = 0; globalThis.__rctDeviceEventEmitter = {emit() { late++; }, listenerCount() { return 1; }};");
  auto invoker = std::make_shared<DeferredInvoker>();
  auto accessibility = std::make_shared<rnqnx::runtime::QtAccessibilityInfoModule>(invoker, fixture.adapter);
  auto back = std::make_shared<rnqnx::runtime::QtDeviceEventManagerModule>(invoker, fixture.adapter);
  QAccessible::setActive(false);
  QAccessible::setActive(true);
  key(fixture.window, Qt::Key_Back);
  require(invoker->tasks.size() >= 2, "Lifetime negative control did not enqueue work");
  accessibility->invalidate(); accessibility->invalidate();
  back->invalidate(); back->invalidate();
  invoker->drain(fixture.host->runtime());
  fixture.check("late === 0");
  accessibility.reset(); back.reset();
  QAccessible::setActive(false);
  key(fixture.window, Qt::Key_Back);
  require(invoker->tasks.empty(), "Closed producers still emitted work");
}
#endif

void contract(const std::string& bundle, const std::string& platform) {
  Fixture fixture;
  require(fixture.host->evaluate(bundle, "platform-services.bundle.js"), fixture.host->lastError().toStdString());
  fixture.eval("globalThis.p = globalThis.__rnqnxPlatformServicesProbe;");
  fixture.check("p.platform === \"" + platform + "\"");
  fixture.check("p.reactNativeVersion.major === 0 && p.reactNativeVersion.minor === 88 && p.reactNativeVersion.patch === 1 && p.reactNativeVersion.prerelease === null");
  fixture.eval("p.accessibility.isReduceMotionEnabled().catch(error => { p.motionError = error.code; });");
  fixture.pump();
  fixture.check("p.motionError === \"ERR_QT_ACCESSIBILITY_UNSUPPORTED\"");
#if RNQNX_ENABLE_PLATFORM_SERVICES
  fixture.eval("p.installBack(); p.installAccessibility();");
  fixture.pump();
  fixture.check(std::string("p.reader === ") + (QAccessible::isActive() ? "true" : "false"));
  const bool highContrast = QGuiApplication::styleHints()->accessibility()->contrastPreference() == Qt::ContrastPreference::HighContrast;
  fixture.check(std::string("p.highContrast === ") + (highContrast ? "true" : "false"));
  fixture.eval("p.firstReader.length = 0; p.secondReader.length = 0;");
  QAccessible::setActive(false); QAccessible::setActive(true);
  fixture.pump();
  fixture.check("JSON.stringify(p.firstReader) === \"[false,true]\" && JSON.stringify(p.secondReader) === JSON.stringify(p.firstReader)");
  fixture.eval("p.first.remove(); p.first.remove(); p.firstReader.length = 0; p.secondReader.length = 0;");
  QAccessible::setActive(false);
  fixture.pump();
  fixture.check("p.firstReader.length === 0 && JSON.stringify(p.secondReader) === \"[false]\"");
  require(QMetaObject::invokeMethod(const_cast<QAccessibilityHints*>(QGuiApplication::styleHints()->accessibility()),
      "contrastPreferenceChanged", Qt::DirectConnection, Q_ARG(Qt::ContrastPreference, Qt::ContrastPreference::HighContrast)),
      "Missing real Qt contrast signal");
  fixture.pump();
  fixture.check("JSON.stringify(p.contrast) === \"[true]\"");
  key(fixture.window, Qt::Key_Back);
  fixture.pump();
  fixture.check("JSON.stringify(p.backEvents) === \"[\\\"last\\\"]\" && p.timestamp === 1250");
  require(fixture.window.isVisible(), "Handled back closed the window");
  key(fixture.window, Qt::Key_Back, true);
  key(fixture.window, Qt::Key_Back, false, Qt::ControlModifier);
  key(fixture.window, Qt::Key_Escape);
  fixture.pump();
  fixture.check("p.backEvents.length === 1");

  fixture.host->startSurface(11, "RNQnxPlatformServicesProbe", 320, 240);
  fixture.until([&] { return fixture.layer.mountedCount() >= 3; }, "React focus target did not mount");
  fixture.eval("p.tagValue = p.tag(); p.staleTag = p.tagValue;");
  const auto tag = fixture.host->runtime().global().getPropertyAsObject(fixture.host->runtime(), "p").getProperty(fixture.host->runtime(), "tagValue").asNumber();
  auto* target = fixture.layer.itemForTag(static_cast<facebook::react::Tag>(tag));
  require(target != nullptr, "Public findNodeHandle did not name a mounted item");
  auto* targetInterface = QAccessible::queryAccessibleInterface(target);
  require(targetInterface && targetInterface->actionInterface(), "Mounted View has no public Qt action interface");
  require(targetInterface->role() == QAccessible::ComboBox && targetInterface->text(QAccessible::Value) == QStringLiteral("Ready") &&
      targetInterface->state().expandable && targetInterface->state().collapsed,
      "Public role precedence, value text or expanded props were filtered before native semantics");
  require(targetInterface->parent() && targetInterface->parent()->role() == QAccessible::Dialog && targetInterface->parent()->state().modal,
      "Public dialog/modal semantics did not reach the Qt tree");
  auto* actions = targetInterface->actionInterface();
  require(actions->actionNames().contains(QAccessibleActionInterface::showMenuAction()), "RN longpress is not advertised as Qt showMenuAction");
  actions->doAction(QAccessibleActionInterface::pressAction());
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  actions->doAction(QAccessibleActionInterface::increaseAction());
  actions->doAction(QAccessibleActionInterface::decreaseAction());
  fixture.pump();
  fixture.check("JSON.stringify(p.actionEvents) === JSON.stringify([\"activate\", \"longpress\", \"increment\", \"decrement\"])");
  fixture.eval("p.setDisabled(true);");
  fixture.until([&] { return targetInterface->state().disabled; }, "Disabled state did not reach the Qt semantic interface");
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  fixture.pump();
  fixture.check("p.actionEvents.length === 4");
  fixture.eval("p.setHidden(true);");
  fixture.until([&] { return targetInterface->state().invisible; }, "Hidden semantic subtree remained exposed");
  fixture.eval("p.setHidden(false);");
  fixture.until([&] { return !targetInterface->state().invisible; }, "Hidden semantic subtree failed to restore");
  fixture.eval("p.setDisabled(false); p.setActions(false);");
  fixture.until([&] { return !targetInterface->state().disabled && !actions->actionNames().contains(QAccessibleActionInterface::showMenuAction()); }, "Removed RN actions remained advertised");
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  fixture.pump();
  fixture.check("p.actionEvents.length === 4");
  QAccessible::setActive(true);
  ObservedEvents events;
  observations = &events;
  const auto previous = QAccessible::installUpdateHandler(observe);
  struct Restore {
    QAccessible::UpdateHandler previous;
    ~Restore() { QAccessible::installUpdateHandler(previous); observations = nullptr; }
  } restore{previous};
  fixture.eval("p.focus(); p.accessibility.announceForAccessibilityWithOptions(\"Qt announcement\", {priority: \"high\"});");
  fixture.pump();
  require(target->hasActiveFocus(), "Public AccessibilityInfo did not focus the mounted Qt item");
  require(events.announcements == 1 && events.message == QStringLiteral("Qt announcement") &&
      events.politeness == QAccessible::AnnouncementPoliteness::Assertive, "Qt announcement payload was not delivered");
  require(events.focus >= 1, "Qt accessibility focus event was not delivered");
  fixture.eval("p.backEvents.length = 0; p.setModal(true);");
  fixture.until([&] { return fixture.layer.mountedCount() >= 5; }, "React modal did not mount");
  fixture.pump();
  fixture.check("p.modalShows === 1");
  key(fixture.window, Qt::Key_Back);
  fixture.until([&] { return fixture.layer.mountedCount() == 3; }, "Modal onRequestClose did not unmount");
  fixture.check("p.modalCloses === 1 && p.backEvents.length === 0");
  fixture.eval("p.setTarget(false);");
  fixture.until([&] { return fixture.layer.mountedCount() == 2; }, "Focus target did not unmount");
  fixture.eval(R"JS(
    const invalid = [
      () => p.accessibility.setAccessibilityFocus(p.staleTag),
      () => p.accessibility.setAccessibilityFocus(-1),
      () => p.accessibility.setAccessibilityFocus(NaN),
      () => p.accessibility.sendAccessibilityEvent(null, 'focus'),
      () => p.accessibility.sendAccessibilityEvent(null, 'click'),
      () => p.accessibility.announceForAccessibility(42),
      () => p.accessibility.announceForAccessibilityWithOptions('text', {queue: true}),
      () => p.accessibility.addEventListener('reduceMotionChanged', () => {}),
      () => p.back.addEventListener('wrong', () => {}),
      () => p.back.addEventListener('hardwareBackPress', null),
    ];
    for (const call of invalid) {
      let rejected = false;
      try { call(); } catch (error) { rejected = error instanceof Error; }
      if (!rejected) throw new Error('Invalid or unsupported native operation did not fail');
    }
    p.handled = false;
  )JS");
  key(fixture.window, Qt::Key_Back);
  fixture.until([&] { return !fixture.window.isVisible(); }, "Unhandled back did not close the owning window");
  fixture.check("JSON.stringify(p.backEvents) === \"[\\\"last\\\",\\\"first\\\"]\"");
  fixture.eval("p.lastBack.remove(); p.lastBack.remove(); p.firstBack.remove(); p.second.remove(); p.contrastSub.remove();");
  fixture.host->stopSurface(11);
  fixture.pump();
#else
  fixture.eval(R"JS(
    p.accessibility.isScreenReaderEnabled().catch(error => { p.unavailable = error.code; });
    let rejected = false;
    try { p.back.addEventListener('hardwareBackPress', () => true); } catch (error) {
      rejected = error instanceof Error && error.message.includes('RNQNX_ENABLE_PLATFORM_SERVICES');
    }
    if (!rejected) throw new Error('Disabled BackHandler did not report its missing provider');
  )JS");
  fixture.pump();
  fixture.check("p.unavailable === \"ERR_QT_ACCESSIBILITY_UNAVAILABLE\"");
#endif
}
}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  try {
    require(argc == 3, "Expected bundle and platform");
    QFile file(QString::fromLocal8Bit(argv[1]));
    require(file.open(QIODevice::ReadOnly), "Cannot read production Metro bundle");
    contract(file.readAll().toStdString(), argv[2]);
#if RNQNX_ENABLE_PLATFORM_SERVICES
    lifetime();
#endif
    std::printf("QT_PLATFORM_SERVICES_OK platform=%s enabled=%d real-rn=1 synthetic-qt=1 device=0\n", argv[2], RNQNX_ENABLE_PLATFORM_SERVICES);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "QT_PLATFORM_SERVICES_FAILURE %s\n", error.what());
    return 1;
  }
}
