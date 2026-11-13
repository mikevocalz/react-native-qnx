#pragma once

#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/runtime/QnxExceptionsManager.h"
#include "react-native-qnx/runtime/QtRuntimeTaskQueue.h"

#include <react/renderer/core/EventBeat.h>
#include <react/renderer/scheduler/Scheduler.h>
#include <react/renderer/scheduler/SchedulerDelegate.h>
#include <react/renderer/scheduler/SurfaceHandler.h>
#include <react/renderer/runtimescheduler/RuntimeScheduler.h>
#include <react/utils/ContextContainer.h>
#include <react/renderer/componentregistry/ComponentDescriptorProvider.h>

#include <ReactCommon/CallInvoker.h>
#include <ReactCommon/TurboModule.h>
#include <folly/dynamic.h>
#include <jsi/jsi.h>

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace rnqnx::runtime {

/// Drives React Native's event beat from the Qt event loop.
///
/// React Native ships no generic implementation: the only two are Android's
/// Choreographer beat and Apple's CFRunLoop observer. The contract
/// (react/renderer/core/EventBeat.h:88-113) is that `request()` asks the host
/// platform for a tick and the platform later calls `induce()`, which forwards
/// the beat callback to the JavaScript thread through the RuntimeScheduler.
class QtEventBeat final : public facebook::react::EventBeat {
 public:
  QtEventBeat(
      std::shared_ptr<OwnerBox> ownerBox,
      facebook::react::RuntimeScheduler& runtimeScheduler);

  void request() const override;

 private:
  /// The receiver for queued beats. Owning it here is what bounds a posted
  /// `induce()` to this beat's lifetime; see request().
  std::unique_ptr<QObject> context_{std::make_unique<QObject>()};
};

/// Applies mount transactions to Qt Quick items.
///
/// Mounting happens in `schedulerShouldRenderTransactions`, not in
/// `schedulerDidFinishTransaction`. The latter fires synchronously on the commit
/// thread for every transaction (Scheduler.cpp:311) and Apple treats it as a
/// no-op; the former is dispatched through the RuntimeScheduler's rendering
/// update (Scheduler.cpp:322-332), which is where a platform is meant to mount.
///
/// The hooks this platform does not implement yet are explicit no-ops with a
/// recorded reason rather than silent stubs. None of them can be omitted:
/// SchedulerDelegate declares thirteen pure virtuals.
class QtSchedulerDelegate final : public facebook::react::SchedulerDelegate {
 public:
  explicit QtSchedulerDelegate(fabric::ShadowViewMutationAdapter& adapter);

  /// Borrowed, with the same lifetime rule as the adapter's: it must outlive
  /// every transaction, including the empty-tree commit that stopping a surface
  /// produces during teardown.
  void setTouchDispatcher(fabric::QtTouchDispatcher* dispatcher);

  /// Hands work to the owner thread. Library code that commits to the shadow
  /// tree from another thread (the UI-worklet thread) triggers mounting and
  /// synchronous prop updates there; Qt items may only change on the owner
  /// thread, so those callbacks are re-posted through this function. Unset
  /// (the default), a callback from a foreign thread is a programming error.
  using OwnerPoster = std::function<void(std::function<void()>)>;
  void setOwnerPoster(OwnerPoster poster);

  using MountReporter = std::function<void(facebook::react::SurfaceId)>;
  void setMountReporter(MountReporter reporter);

  /// Failures from the most recent mount, empty when everything applied.
  const std::vector<fabric::MutationFailure>& lastFailures() const;

  /// How many transactions have been mounted, for tests that need to know a
  /// transaction actually arrived rather than that the tree happens to match.
  int mountedTransactionCount() const;

  void schedulerDidFinishTransaction(
      const std::shared_ptr<const facebook::react::MountingCoordinator>&
          mountingCoordinator) override;
  void schedulerShouldRenderTransactions(
      const std::shared_ptr<const facebook::react::MountingCoordinator>&
          mountingCoordinator) override;
  void schedulerShouldMergeReactRevision(
      facebook::react::SurfaceId surfaceId) override;
  void schedulerDidRequestPreliminaryViewAllocation(
      const facebook::react::ShadowNode& shadowNode) override;
  void schedulerDidDispatchCommand(
      const facebook::react::ShadowView& shadowView,
      const std::string& commandName,
      const folly::dynamic& args) override;
  void schedulerDidSendAccessibilityEvent(
      const facebook::react::ShadowView& shadowView,
      const std::string& eventType) override;
  void schedulerDidSetIsJSResponder(
      const facebook::react::ShadowView& shadowView,
      bool isJSResponder,
      bool blockNativeResponder) override;
  void schedulerShouldSynchronouslyUpdateViewOnUIThread(
      facebook::react::Tag tag,
      const folly::dynamic& props) override;
  void schedulerDidUpdateShadowTree(
      const std::unordered_map<facebook::react::Tag, folly::dynamic>& tagToProps)
      override;
  void schedulerDidCaptureViewSnapshot(
      facebook::react::Tag tag,
      facebook::react::SurfaceId surfaceId) override;
  void schedulerDidSetViewSnapshot(
      facebook::react::Tag sourceTag,
      facebook::react::Tag targetTag,
      facebook::react::SurfaceId surfaceId) override;
  void schedulerDidClearPendingSnapshots() override;

 private:
  fabric::ShadowViewMutationAdapter& adapter_;
  /// True when called on the owner thread. Otherwise `work` has been posted to
  /// the owner and the caller must return.
  bool onOwnerOrPost(std::function<void()> work);

  const std::thread::id owner_{std::this_thread::get_id()};
  OwnerPoster ownerPoster_;
  MountReporter mountReporter_;
  fabric::QtTouchDispatcher* touchDispatcher_{nullptr};
  std::vector<fabric::MutationFailure> lastFailures_;
  int mountedTransactionCount_{0};
};

/// Registers the component descriptors this platform can mount.
///
/// React Native declares `getDefaultComponentRegistryFactory()`
/// (componentregistry/ComponentDescriptorFactory.h:29) and never defines it
/// anywhere in the package, so every platform supplies its own. Unknown
/// components resolve to the unimplemented-native-view descriptor rather than throwing
/// `std::invalid_argument` out of ComponentDescriptorRegistry::at.
/// The component names this platform registers. One list, shared by the
/// registry factory and the JavaScript component predicate so they cannot
/// drift apart.
std::vector<std::string> registeredComponentNames();

facebook::react::ComponentRegistryFactory makeComponentRegistryFactory();

/// Owns a Hermes runtime and the React Native scheduler wired to Qt Quick.
///
/// This is the layer React Native has no generic implementation of: the package
/// ships `react/runtime/platform/ios` and nothing else, so the host wiring is
/// ours. JavaScript runs on the thread that constructs this object; there is no
/// separate JS thread yet, which keeps the first bring-up honest about what has
/// actually been proven.
class QtReactHost {
 public:
  QtReactHost(fabric::ShadowViewMutationAdapter& adapter);
#if RNQNX_ENABLE_FRAME_TRACE
  /// Native diagnostics only. Caller retains the session through host teardown,
  /// then freezes/exports it outside runtime callbacks.
  QtReactHost(fabric::ShadowViewMutationAdapter& adapter,
              std::shared_ptr<RuntimeTrace> trace);
#endif
  ~QtReactHost();

  QtReactHost(const QtReactHost&) = delete;
  QtReactHost& operator=(const QtReactHost&) = delete;
  QtReactHost(QtReactHost&&) = delete;
  QtReactHost& operator=(QtReactHost&&) = delete;

  facebook::jsi::Runtime& runtime();
  /// Shared native completion route; closes before the host's Hermes teardown.
  std::shared_ptr<QtRuntimeTaskQueue> taskQueue() const;
  facebook::react::Scheduler& scheduler();
  QtSchedulerDelegate& delegate();

  /// Evaluates JavaScript on the runtime. Returns false and fills
  /// `lastError()` when the script throws. Must be called on the owning
  /// thread at an idle JS boundary; reentrant evaluation returns false.
  bool evaluate(const std::string& source, const std::string& sourceUrl);

  /// True when `global.nativeFabricUIManager` is installed, which is what
  /// proves the UIManagerBinding reached the runtime.
  bool hasFabricUIManager();

  /// Installs the predicate JavaScript view configs use to ask whether a native
  /// component exists
  /// (componentregistry/native/NativeComponentRegistryBinding.h:26). React
  /// Native ships this only for Android and Apple. Names are normalised through
  /// `componentNameByReactViewName` first, because JavaScript asks for `RCTView`
  /// where the registry holds `View`.
  void installComponentProvider();

  /// Installs the TurboModule binding React Native's own JavaScript needs.
  ///
  /// This is the "minimal internal adapter" ADR-0004 sanctions, not a move to
  /// TurboModules: application-facing APIs are Nitro HybridObjects. React
  /// Native's core JavaScript calls `TurboModuleRegistry.getEnforcing` for its
  /// own modules, and nothing else can answer that registry.
  ///
  /// The provider delegates to `DefaultTurboModules::getTurboModule`, which
  /// returns React Native's own generic C++ modules — real implementations
  /// backed by the platform runtime, not stubs. Anything it does not cover
  /// returns nullptr so `getEnforcing` fails with the module's name, which
  /// ADR-0004 requires over empty TurboModules that silence the error.
  void installTurboModuleBinding();

  /// Factory for a TurboModule that a platform library (not React Native core)
  /// needs under a fixed name, such as react-native-worklets' `WorkletsModule`.
  /// The argument is the host's CallInvoker for the module's JS-side calls.
  using TurboModuleFactory =
      std::function<std::shared_ptr<facebook::react::TurboModule>(
          std::shared_ptr<facebook::react::CallInvoker>)>;

  /// Adds a Fabric component descriptor (and the component's name) to the set
  /// every host creates. Process-wide, because React Native keeps its provider
  /// registry in a function-local static; call before the first QtReactHost is
  /// constructed. Duplicate names throw. Mapping the component to a Qt Quick
  /// type is separate (`ShadowViewMutationAdapter::mapComponent`).
  static void addComponentDescriptorProvider(
      std::string name, facebook::react::ComponentDescriptorProvider provider);

  /// Registers `factory` for `name`. Must be called before
  /// `installTurboModuleBinding()`. Registered names win over the built-in
  /// table, so a library cannot be shadowed by a stub; a duplicate name throws.
  void registerTurboModule(std::string name, TurboModuleFactory factory);

  /// Runs `hook` once, on the owner thread, while Hermes and the scheduler are
  /// still alive: after every surface has stopped and before the JavaScript
  /// task queue closes. Hooks run in reverse registration order. Libraries that
  /// own threads or runtimes use this to stop them before the host's runtime
  /// is destroyed.
  void addTeardownHook(std::function<void()> hook);

  /// See `QtSchedulerDelegate::setOwnerPoster`.
  void setOwnerPoster(QtSchedulerDelegate::OwnerPoster poster);
  /// Supplies extra TurboModules, asked before the platform's own. Returning
  /// nullptr falls through to the built-in modules. Set it before
  /// installTurboModuleBinding(); the development client uses it for
  /// DevSettings, DevLoadingView and LogBox.
  using TurboModuleProvider =
      std::function<std::shared_ptr<facebook::react::TurboModule>(
          const std::string& name,
          const std::shared_ptr<facebook::react::CallInvoker>& jsInvoker)>;
  void setTurboModuleProvider(TurboModuleProvider provider);

  /// Calls `method` on a module JavaScript registered with
  /// registerCallableModule (HMRClient, RCTDeviceEventEmitter, ...), the
  /// native-to-JavaScript direction ReactInstance::callFunctionOnModule covers
  /// on Android and iOS. Runs as a RuntimeScheduler task; an unknown module or
  /// method is reported like any other task error.
  void callFunctionOnModule(
      const std::string& moduleName,
      const std::string& methodName,
      folly::dynamic args);

  /// Installs `globalThis.globalEvalWithSourceUrl(source, url)`. Metro's HMR
  /// client evaluates each hot update through it so stack traces name the
  /// edited file. Needs a Hermes build that can compile source (not the lean
  /// VM); development builds only.
  void installGlobalEvalWithSourceUrl();

  /// Errors JavaScript has reported through ExceptionsManager, oldest first.
  const std::vector<JavaScriptError>& javaScriptErrors() const;

  /// Installs `globalThis.__RNQNX_PLATFORM_CONSTANTS`, which the platform
  /// package's `Platform.js` reads. Values are real: the OS name and version
  /// come from the running system, not from a placeholder, and the platform
  /// module throws rather than inventing them if this was never called.
  void installPlatformConstants();

  /// Installs the timer functions JavaScript expects from its host. Hermes is a
  /// JavaScript engine, not a runtime: `setTimeout` and friends are the host's
  /// job, and React needs them. These are backed by `QTimer`, so they fire on
  /// the Qt event loop — which means they only fire when that loop is running.
  void installTimers();

  /// Runs pending Qt events, so timers scheduled by JavaScript can fire in a
  /// context with no running event loop, such as a test.
  void pumpEvents();

  /// Whether a component name resolves after normalisation. Exposed so a test
  /// can assert the predicate directly rather than only through JavaScript.
  bool hasComponent(const std::string& componentName) const;

  /// Starts a surface with no JavaScript module attached. React Native calls
  /// this an empty surface (SurfaceHandler.cpp:72); it exists so a platform can
  /// prove its mounting path before a bundle is involved.
  ///
  /// Stopping any surface still calls into JavaScript:
  /// `UIManager::stopSurface` runs `AppRegistryBinding::stopSurface`
  /// unconditionally (UIManager.cpp:292-294), which throws unless
  /// `global.RN$stopSurface` exists (AppRegistryBinding.cpp). An empty surface
  /// has no React app to tear down, so this installs a no-op for it. A real
  /// bundle overwrites the global with React's own implementation
  /// (Libraries/Renderer/shims/ReactFabric.js:27), and this shim goes away with
  /// the first real bundle.
  void startEmptySurface(
      facebook::react::SurfaceId surfaceId,
      float width,
      float height);

  /// Starts a surface backed by a registered JavaScript component. This is the
  /// path a real app takes: `UIManager::startSurface` hops the runtime executor
  /// and calls `AppRegistryBinding::startSurface`, which invokes
  /// `global.RN$AppRegistry.runApplication` (AppRegistryBinding.cpp:14-40). The
  /// bundle must already be evaluated.
  void startSurface(
      facebook::react::SurfaceId surfaceId,
      const std::string& moduleName,
      float width,
      float height);
  void stopSurface(facebook::react::SurfaceId surfaceId);
  /// Lays a running surface out at a new size, for a window that was resized,
  /// maximized or restored. React sees it as a new layout of the root view
  /// (onLayout fires); nothing remounts. Unknown surfaces are ignored.
  void resizeSurface(facebook::react::SurfaceId surfaceId, float width, float height);

  /// Commits `children` as the surface root's children through the UIManager's
  /// shadow tree, which is the same path a React commit takes.
  bool commit(
      facebook::react::SurfaceId surfaceId,
      const std::vector<std::shared_ptr<const facebook::react::ShadowNode>>&
          children);

  const std::shared_ptr<const facebook::react::ContextContainer>&
  contextContainer() const;

  QString lastError() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace rnqnx::runtime
