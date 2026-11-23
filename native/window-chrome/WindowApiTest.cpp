// Host tests for the GUI side of the Nitro windows API: the close gate, the
// state publisher, the guard sequencer and QtWindowServices. Qt offscreen; no
// Nitro or Hermes (the adapter's JS side is covered by the nitro-v1-host
// compile lane and the device receipt). Labelled host.

#include "CloseGuardSequencer.hpp"
#include "HostWindowState.h"
#include "QtWindowServices.h"
#include "WindowChrome.h"
#include "WindowCloseGate.h"
#include "WindowStatePublisher.h"
#include "react-native-qnx/runtime/QueuedCallTarget.h"

#include <QCoreApplication>
#include <qpa/qwindowsysteminterface.h>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

using namespace rnqnx::window;
using rnqnx::nitro::CloseGuardSequencer;

namespace {

// One real window with a gate and a recording request handler.
struct GateRig {
  GateRig() {
    window.setTitle(QStringLiteral("Gate"));
    window.resize(320, 240);
    window.show();
    gate = std::make_unique<WindowCloseGate>(window);
    gate->setRequestHandler([this](const CloseRequestInfo& info) { requests.push_back(info); });
  }
  QQuickWindow window;
  std::unique_ptr<WindowCloseGate> gate;
  std::vector<CloseRequestInfo> requests;
};

}  // namespace

class WindowApiTest : public QObject {
  Q_OBJECT

 private Q_SLOTS:
  // ---- close gate -------------------------------------------------------

  void closeWithNoGuardNeedsNoJavaScript() {
    GateRig rig;
    // The handler stands for a blocked JS thread: it must not be asked.
    QVERIFY(rig.window.close());
    QVERIFY(!rig.window.isVisible());
    QCOMPARE(rig.requests.size(), std::size_t{0});
  }

  void closeWithGuardAsksAndWaits() {  // negative control for the test above
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->noteNextClose(CloseReason::Control, true, -1);  // the control announces itself
    rig.window.close();
    QVERIFY2(rig.window.isVisible(), "a guarded close must wait for the guards");
    QCOMPARE(rig.requests.size(), std::size_t{1});
    QCOMPARE(rig.requests[0].reason, CloseReason::Control);
    QCOMPARE(rig.requests[0].guardIds, std::vector<std::uint64_t>{1});
  }

  void guardCancelsKeepsWindow() {
    GateRig rig;
    rig.gate->addGuard(1);
    std::vector<bool> results;
    rig.gate->requestClose(false, [&](bool accepted) { results.push_back(accepted); });
    QCOMPARE(rig.requests.size(), std::size_t{1});
    QCOMPARE(rig.requests[0].reason, CloseReason::App);
    rig.gate->settle(rig.requests[0].requestId, rig.requests[0].epoch, false);
    QCOMPARE(results, std::vector<bool>{false});
    QVERIFY(rig.window.isVisible());
    QVERIFY(!rig.gate->pending());
    // The next close asks again.
    rig.window.close();
    QCOMPARE(rig.requests.size(), std::size_t{2});
  }

  void guardProceedsClosesAfterSettling() {
    GateRig rig;
    rig.gate->addGuard(1);
    bool visibleWhenSettled = false;
    std::vector<bool> results;
    rig.gate->requestClose(false, [&](bool accepted) {
      visibleWhenSettled = rig.window.isVisible();
      results.push_back(accepted);
    });
    rig.gate->settle(rig.requests[0].requestId, rig.requests[0].epoch, true);
    QCOMPARE(results, std::vector<bool>{true});
    QVERIFY2(visibleWhenSettled, "the Promise settles before the native close is posted");
    QVERIFY(!rig.window.isVisible());
  }

  void repeatedCloseJoinsThePendingOne() {
    GateRig rig;
    rig.gate->addGuard(1);
    std::vector<bool> a, b;
    rig.gate->requestClose(false, [&](bool r) { a.push_back(r); });
    rig.window.close();  // the close control
    rig.gate->requestClose(false, [&](bool r) { b.push_back(r); });
    QCOMPARE(rig.requests.size(), std::size_t{1});  // one guard run
    rig.gate->settle(rig.requests[0].requestId, rig.requests[0].epoch, true);
    QCOMPARE(a, std::vector<bool>{true});
    QCOMPARE(b, std::vector<bool>{true});
  }

  void forceSkipsGuardsAndOverridesPending() {
    GateRig rig;
    rig.gate->addGuard(1);
    std::vector<bool> joined;
    rig.gate->requestClose(false, [&](bool r) { joined.push_back(r); });
    const CloseRequestInfo pending = rig.requests[0];
    rig.gate->requestClose(true, [&](bool r) { joined.push_back(r); });
    QCOMPARE(joined, (std::vector<bool>{true, true}));
    QVERIFY(!rig.window.isVisible());
    // A late guard result is ignored.
    rig.gate->settle(pending.requestId, pending.epoch, false);
    QCOMPARE(rig.requests.size(), std::size_t{1});
  }

  void forceWithNothingPendingDoesNotAskGuards() {
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->requestClose(true, {});
    QVERIFY(!rig.window.isVisible());
    QCOMPARE(rig.requests.size(), std::size_t{0});
  }

  void reloadDuringGuardsLetsTheCloseProceed() {
    GateRig rig;
    rig.gate->addGuard(1);
    std::vector<bool> results;
    rig.gate->requestClose(false, [&](bool r) { results.push_back(r); });
    const CloseRequestInfo old = rig.requests[0];
    rig.gate->resetRuntime();  // teardown hook: epoch bump, ids cleared
    QCOMPARE(results, std::vector<bool>{true});
    QVERIFY(!rig.window.isVisible());
    QCOMPARE(rig.gate->guardCount(), std::size_t{0});
    rig.gate->settle(old.requestId, old.epoch, false);  // late result: dropped, no crash
  }

  void staleGuardResultCannotAnswerANewRequest() {  // teardown race
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->requestClose(false, {});
    const CloseRequestInfo old = rig.requests[0];
    rig.gate->resetRuntime();
    // The window was closed by the reload; reopen and register a new guard.
    rig.window.show();
    rig.gate->addGuard(2);
    rig.gate->requestClose(false, {});
    QCOMPARE(rig.requests.size(), std::size_t{2});
    QVERIFY(rig.gate->pending());
    // The old runtime's answer arrives late, with the new request's id.
    rig.gate->settle(rig.requests[1].requestId, old.epoch, true);
    QVERIFY2(rig.gate->pending(), "an answer from an old epoch must be dropped");
    QVERIFY(rig.window.isVisible());
    rig.gate->settle(rig.requests[1].requestId, rig.requests[1].epoch, true);
    QVERIFY(!rig.window.isVisible());
  }

  void epochBumpedBySharedCounterDropsThePost() {
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->requestClose(false, {});
    const CloseRequestInfo info = rig.requests[0];
    rig.gate->epochCounter()->fetch_add(1);  // what the JS teardown hook does first
    rig.gate->settle(info.requestId, info.epoch, true);
    QVERIFY(rig.window.isVisible());
    // The GUI side then notices on the next close event: proceeds unguarded.
    rig.window.close();
    QVERIFY(!rig.window.isVisible());
  }

  void systemCloseThatCannotBeCancelledClosesWhenGuardsSettle() {
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->noteNextClose(CloseReason::System, false, 3000);
    rig.window.close();
    QCOMPARE(rig.requests.size(), std::size_t{1});
    QCOMPARE(rig.requests[0].reason, CloseReason::System);
    QVERIFY(!rig.requests[0].cancellable);
    QCOMPARE(rig.requests[0].deadlineMs, 3000);
    rig.gate->settle(rig.requests[0].requestId, rig.requests[0].epoch, false);
    QVERIFY2(!rig.window.isVisible(), "a close that cannot be cancelled ends when the guards settle");
  }

  void platformCloseIsSystemAndHiddenOneCannotBeCancelled() {
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->setSystemClose(true, -1);
    // What the platform plugin does for a window manager close message.
    QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(&rig.window);
    QCOMPARE(rig.requests.size(), std::size_t{1});
    QCOMPARE(rig.requests[0].reason, CloseReason::System);
    QVERIFY(rig.requests[0].cancellable);
    rig.gate->settle(rig.requests[0].requestId, rig.requests[0].epoch, false);
    QVERIFY(rig.window.isVisible());
    // The manager's switcher can close a minimized window: it must not be cancellable.
    rig.gate->setVisibleProbe([] { return false; });
    QWindowSystemInterface::handleCloseEvent<QWindowSystemInterface::SynchronousDelivery>(&rig.window);
    QCOMPARE(rig.requests.size(), std::size_t{2});
    QVERIFY(!rig.requests[1].cancellable);
    // And a source that cannot be cancelled says so.
    rig.gate->settle(rig.requests[1].requestId, rig.requests[1].epoch, false);
    QVERIFY(!rig.window.isVisible());
  }

  void quitDoesNotFireTheCloseEvent() {  // the --replace path (N5)
    GateRig rig;
    rig.gate->addGuard(1);
    QTimer::singleShot(0, QCoreApplication::instance(), [] { QCoreApplication::quit(); });
    // Failsafe: a quit that waits on a veto would otherwise hang the test.
    QTimer::singleShot(3000, QCoreApplication::instance(), [] { QCoreApplication::exit(99); });
    const int code = QCoreApplication::exec();
    qInfo() << "exec returned" << code << "close requests" << rig.requests.size();
    QCOMPARE(code, 0);
    QCOMPARE(rig.requests.size(), std::size_t{0});
  }

  void removedGuardIsNotAsked() {
    GateRig rig;
    rig.gate->addGuard(1);
    rig.gate->removeGuard(1);
    rig.window.close();
    QVERIFY(!rig.window.isVisible());
    QCOMPARE(rig.requests.size(), std::size_t{0});
  }

  // ---- state publisher --------------------------------------------------

  void publisherSequencesAndSkipsRepeats() {
    WindowStatePublisher publisher;
    WindowState state;
    state.title = "a";
    const auto first = publisher.publish(state);
    QVERIFY(first);
    QCOMPARE(first->sequence, std::uint64_t{1});
    QVERIFY2(!publisher.publish(state), "same values publish nothing");
    state.title = "b";
    QCOMPARE(publisher.publish(state)->sequence, std::uint64_t{2});
    QCOMPARE(publisher.current()->title, std::string("b"));
    QCOMPARE(first->title, std::string("a"));  // old snapshots stay valid
  }

  void publisherCoalescesStatePosts() {
    WindowStatePublisher publisher;
    QVERIFY(publisher.markStatePending());
    QVERIFY(!publisher.markStatePending());
    QVERIFY(!publisher.markStatePending());
    WindowState state;
    state.title = "latest";
    publisher.publish(state);
    QCOMPARE(publisher.takePending()->title, std::string("latest"));
    QVERIFY(publisher.markStatePending());
  }

  void publisherInsetCoalescesAndSkipsRepeats() {
    WindowStatePublisher publisher;
    WindowInset inset;
    inset.top = 32;
    QVERIFY(publisher.publishInset(inset));
    QVERIFY2(!publisher.publishInset(inset), "same inset publishes nothing");
    QCOMPARE(publisher.currentInset()->top, 32.0);
    QVERIFY(publisher.markInsetPending());
    QVERIFY(!publisher.markInsetPending());
    inset.top = 40;
    publisher.publishInset(inset);
    QCOMPARE(publisher.takeInsetPending()->top, 40.0);
    QVERIFY(publisher.markInsetPending());
  }

  // ---- guard sequencer --------------------------------------------------

  void sequencerAsksInOrderAndStopsAtFirstFalse() {
    std::vector<int> asked;
    std::vector<bool> finished;
    CloseGuardSequencer sequencer([&](std::uint64_t, std::uint64_t, bool allow) { finished.push_back(allow); });
    for (int i = 1; i <= 3; ++i) sequencer.add(i, [&asked, i](const CloseRequestInfo&) { asked.push_back(i); });
    CloseRequestInfo request;
    request.requestId = 7;
    request.guardIds = {1, 2, 3};
    sequencer.begin(request);
    QCOMPARE(asked, std::vector<int>{1});
    sequencer.resolve(7, 1, true);
    QCOMPARE(asked, (std::vector<int>{1, 2}));
    sequencer.resolve(7, 2, false);
    QCOMPARE(finished, std::vector<bool>{false});
    QCOMPARE(asked, (std::vector<int>{1, 2}));  // guard 3 never asked
  }

  void sequencerTwoGuardsBothAllowFinishesAllowed() {
    std::vector<bool> finished;
    CloseGuardSequencer sequencer([&](std::uint64_t, std::uint64_t, bool allow) { finished.push_back(allow); });
    sequencer.add(1, [](const CloseRequestInfo&) {});
    sequencer.add(2, [](const CloseRequestInfo&) {});
    CloseRequestInfo request;
    request.requestId = 1;
    request.guardIds = {1, 2};
    sequencer.begin(request);
    sequencer.resolve(1, 1, true);
    QVERIFY(finished.empty());
    sequencer.resolve(1, 2, true);
    QCOMPARE(finished, std::vector<bool>{true});
  }

  void sequencerIgnoresFalseWhenNotCancellable() {
    std::vector<bool> finished;
    CloseGuardSequencer sequencer([&](std::uint64_t, std::uint64_t, bool allow) { finished.push_back(allow); });
    sequencer.add(1, [](const CloseRequestInfo&) {});
    sequencer.add(2, [](const CloseRequestInfo&) {});
    CloseRequestInfo request;
    request.requestId = 1;
    request.cancellable = false;
    request.guardIds = {1, 2};
    sequencer.begin(request);
    sequencer.resolve(1, 1, false);
    sequencer.resolve(1, 2, false);
    QCOMPARE(finished, std::vector<bool>{true});
  }

  void sequencerDropsStaleAndDoubleAnswers() {
    std::vector<bool> finished;
    CloseGuardSequencer sequencer([&](std::uint64_t, std::uint64_t, bool allow) { finished.push_back(allow); });
    sequencer.add(1, [](const CloseRequestInfo&) {});
    sequencer.add(2, [](const CloseRequestInfo&) {});
    CloseRequestInfo request;
    request.requestId = 5;
    request.guardIds = {1, 2};
    sequencer.begin(request);
    sequencer.resolve(4, 1, true);   // wrong request
    sequencer.resolve(5, 2, true);   // guard 2 is not the one asked
    QVERIFY(finished.empty());
    sequencer.resolve(5, 1, true);
    sequencer.resolve(5, 1, false);  // double answer from guard 1
    QVERIFY(finished.empty());
    sequencer.resolve(5, 2, true);
    QCOMPARE(finished, std::vector<bool>{true});
  }

  void sequencerRemovedGuardsAreSkippedOrCountAsAllow() {
    std::vector<int> asked;
    std::vector<bool> finished;
    CloseGuardSequencer sequencer([&](std::uint64_t, std::uint64_t, bool allow) { finished.push_back(allow); });
    for (int i = 1; i <= 3; ++i) sequencer.add(i, [&asked, i](const CloseRequestInfo&) { asked.push_back(i); });
    CloseRequestInfo request;
    request.requestId = 1;
    request.guardIds = {1, 2, 3};
    sequencer.begin(request);
    sequencer.remove(2);  // not yet asked: skipped
    sequencer.remove(1);  // being asked: counts as allow
    QCOMPARE(asked, (std::vector<int>{1, 3}));
    sequencer.resolve(1, 3, true);
    QCOMPARE(finished, std::vector<bool>{true});
  }

  void sequencerClearDropsTheRequestWithoutReporting() {
    bool finished = false;
    CloseGuardSequencer sequencer([&](std::uint64_t, std::uint64_t, bool) { finished = true; });
    sequencer.add(1, [](const CloseRequestInfo&) {});
    CloseRequestInfo request;
    request.requestId = 1;
    request.guardIds = {1};
    sequencer.begin(request);
    sequencer.clear();
    sequencer.resolve(1, 1, true);
    QVERIFY(!finished);
  }

  // ---- services over a real chrome --------------------------------------

  void servicesPublishStateAndOrderEvents() {
    ServicesRig rig;
    auto& s = *rig.services->services();
    QCOMPARE(QString::fromStdString(s.state->current()->title), QStringLiteral("Svc"));
    const auto before = s.state->current()->sequence;
    std::string error = "unset";
    s.setTitle("Renamed", [&](std::string e) { error = e; });
    QVERIFY(error.empty());
    QCOMPARE(QString::fromStdString(s.state->current()->title), QStringLiteral("Renamed"));
    QVERIFY(s.state->current()->sequence > before);

    // Minimize flips visibility, then focus. Both are delivered, none coalesced.
    rig.window.requestActivate();
    QTRY_VERIFY(rig.window.isActive());
    rig.events.clear();
    rig.window.hide();
    QCOMPARE(rig.events, (QStringList{QStringLiteral("visible=false"), QStringLiteral("focused=false")}));
    rig.window.show();
    rig.window.requestActivate();
    QTRY_VERIFY(rig.events.contains(QStringLiteral("focused=true")));
    QVERIFY(rig.events.indexOf(QStringLiteral("visible=true")) < rig.events.indexOf(QStringLiteral("focused=true")));
  }

  void servicesPublishTheTitleBarInset() {
    WindowChromeConfig config;
    config.titleBarStyle = TitleBarStyle::Hidden;
    ServicesRig rig(config);
    auto& s = *rig.services->services();
    // The controls float over the content: the strip they sit in is the inset.
    const auto inset = s.state->currentInset();
    QVERIFY(inset->top > 0);
    QCOMPARE(inset->top, rig.chrome->titleBarInset().value(QStringLiteral("top")).toDouble());
    QVERIFY(inset->overlayWidth > 0);
    // A layout change that moves the controls republishes and notifies once per change.
    QVERIFY(s.state->publishInset(WindowInset{}));  // pretend JS saw a different inset
    rig.insetEvents = 0;
    rig.chrome->setControlsLayout(QStringLiteral("windows"));
    QTRY_VERIFY(rig.insetEvents >= 1);
    QVERIFY(s.state->currentInset()->overlayWidth > 0);
  }

  void servicesDefaultStyleHasNoInset() {
    ServicesRig rig;
    QCOMPARE(rig.services->services()->state->currentInset()->top, 0.0);
  }

  void servicesSetBoundsClampsAndReports() {
    ServicesRig rig;
    auto& s = *rig.services->services();
    QVERIFY(std::find(s.capabilities.begin(), s.capabilities.end(), "move") != s.capabilities.end());
    rnqnx::nitro::Rect applied;
    std::string error = "unset";
    s.setBounds({10, 20, 50, 40}, [&](std::string e, rnqnx::nitro::Rect r) { error = e; applied = r; });
    QVERIFY(error.empty());
    QCOMPARE(applied.width, 400.0);   // clamped to the minimum
    QCOMPARE(applied.height, 300.0);
    QCOMPARE(s.state->current()->width, 400.0);
  }

  void servicesCrossThreadsOnlyThroughTheGuiTarget() {  // JS thread on
    ServicesRig rig;
    auto& s = *rig.services->services();
    std::atomic<bool> done{false};
    std::atomic<bool> onGuiThread{false};
    const auto guiThread = QThread::currentThread();
    std::thread js([&] {
      s.setTitle("From JS thread", [&](std::string) {
        onGuiThread = QThread::currentThread() == guiThread;
        done = true;
      });
    });
    QTRY_VERIFY(done.load());
    js.join();
    QVERIFY(onGuiThread.load());
    QCOMPARE(QString::fromStdString(s.state->current()->title), QStringLiteral("From JS thread"));
  }

  void servicesControlCloseIsControlAndPlatformCloseIsSystem() {
    ServicesRig rig;
    auto& s = *rig.services->services();
    s.guardAdded(1);
    rig.chrome->trigger(QStringLiteral("close"));
    QCOMPARE(rig.closeRequests.size(), std::size_t{1});
    QCOMPARE(rig.closeRequests[0].reason, CloseReason::Control);
    s.settleClose(rig.closeRequests[0].requestId, rig.closeRequests[0].epoch, false);
    QVERIFY(rig.window.isVisible());
    rig.window.close();  // nobody announced it
    QCOMPARE(rig.closeRequests.size(), std::size_t{2});
    QCOMPARE(rig.closeRequests[1].reason, CloseReason::System);
  }

  void servicesCloseWithGuardRoundTrip() {
    ServicesRig rig;
    auto& s = *rig.services->services();
    s.guardAdded(1);
    std::vector<bool> results;
    s.requestClose(false, [&](bool accepted) { results.push_back(accepted); });
    QCOMPARE(rig.closeRequests.size(), std::size_t{1});
    s.settleClose(rig.closeRequests[0].requestId, rig.closeRequests[0].epoch, false);
    QCOMPARE(results, std::vector<bool>{false});
    QVERIFY(rig.window.isVisible());
    s.requestClose(false, [&](bool accepted) { results.push_back(accepted); });
    s.settleClose(rig.closeRequests[1].requestId, rig.closeRequests[1].epoch, true);
    QCOMPARE(results, (std::vector<bool>{false, true}));
    QVERIFY(!rig.window.isVisible());
  }

  void servicesRuntimeGoneDropsGuardsAndLetsPendingCloseProceed() {
    ServicesRig rig;
    auto& s = *rig.services->services();
    s.guardAdded(1);
    std::vector<bool> results;
    s.requestClose(false, [&](bool accepted) { results.push_back(accepted); });
    s.epoch->fetch_add(1);
    s.runtimeGone();
    QCOMPARE(results, std::vector<bool>{true});
    QVERIFY(!rig.window.isVisible());
  }

 private:
  struct ServicesRig {
    explicit ServicesRig(WindowChromeConfig config = {}) {
      window.setTitle(QStringLiteral("Svc"));
      window.resize(800, 600);
      auto capabilities = capabilitiesForPlatform(QStringLiteral("cocoa"), {}, SurfaceResize::FollowsWindow);
      chrome = std::make_unique<WindowChrome>(window, engine, config, capabilities);
      window.show();
      gate = std::make_unique<WindowCloseGate>(window);
      services = std::make_unique<QtWindowServices>(window, *chrome, *gate, capabilities, false, target);
      rnqnx::nitro::WindowEventHandlers handlers;
      handlers.onCloseRequest = [this](const CloseRequestInfo& info) { closeRequests.push_back(info); };
      handlers.onFocusChanged = [this](bool v) { events << QStringLiteral("focused=%1").arg(v ? "true" : "false"); };
      handlers.onInsetChanged = [this] { ++insetEvents; };
      handlers.onVisibilityChanged = [this](bool v) { events << QStringLiteral("visible=%1").arg(v ? "true" : "false"); };
      services->services()->connect(handlers);
    }
    ~ServicesRig() {
      services.reset();
      gate.reset();
      chrome.reset();
    }
    QQmlEngine engine;
    QQuickWindow window;
    rnqnx::runtime::QueuedCallTarget target;
    std::unique_ptr<WindowChrome> chrome;
    std::unique_ptr<WindowCloseGate> gate;
    std::unique_ptr<QtWindowServices> services;
    std::vector<CloseRequestInfo> closeRequests;
    QStringList events;
    int insetEvents{0};
  };
};

QTEST_MAIN(WindowApiTest)
#include "WindowApiTest.moc"
