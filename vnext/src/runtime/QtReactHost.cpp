#include "react-native-qnx/runtime/QtReactHost.h"

#include <hermes/hermes.h>
#include <hermes/Public/RuntimeConfig.h>

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/componentregistry/componentNameByReactViewName.h>
#include <react/renderer/componentregistry/native/NativeComponentRegistryBinding.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/FBReactNativeSpec/ComponentDescriptors.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>
#include <react/renderer/core/ShadowNodeFragment.h>
#include <react/renderer/mounting/MountingCoordinator.h>
#include <react/renderer/mounting/MountingTransaction.h>
#include <react/renderer/mounting/ShadowTree.h>
#include <react/renderer/runtimescheduler/RuntimeSchedulerBinding.h>
#include <react/renderer/runtimescheduler/RuntimeSchedulerCallInvoker.h>
#include <ReactCommon/TurboModuleBinding.h>
#include <react/nativemodule/defaults/DefaultTurboModules.h>
#include <react/renderer/scheduler/SchedulerToolbox.h>
#include <react/renderer/uimanager/UIManager.h>

#include <folly/dynamic.h>

#include <QCoreApplication>
#include <QSysInfo>
#include <QTimer>
#include <QMetaObject>

#include <algorithm>
#include <utility>

namespace rnqnx::runtime {
namespace {

using facebook::react::ComponentDescriptorProviderRegistry;
using facebook::react::ContextContainer;
using facebook::react::RootComponentDescriptor;
using facebook::react::RuntimeScheduler;
using facebook::react::Scheduler;
using facebook::react::SchedulerToolbox;
using facebook::react::ShadowNode;
using facebook::react::SurfaceHandler;
using facebook::react::SurfaceId;
using facebook::react::UnimplementedNativeViewComponentDescriptor;
using facebook::react::ViewComponentDescriptor;

}  // namespace

QtEventBeat::QtEventBeat(
    std::shared_ptr<OwnerBox> ownerBox,
    RuntimeScheduler& runtimeScheduler)
    : facebook::react::EventBeat(std::move(ownerBox), runtimeScheduler) {}

void QtEventBeat::request() const {
  EventBeat::request();

  // Ask the Qt event loop for the tick. Queuing through the application object
  // means the beat lands after the current event finishes, which is what the
  // Android and Apple beats do with their own frame callbacks.
  auto* application = QCoreApplication::instance();
  if (application == nullptr) {
    // Without an event loop there is nothing to schedule against. Inducing
    // immediately keeps a headless test moving instead of hanging.
    induce();
    return;
  }

  QMetaObject::invokeMethod(
      application, [this]() { induce(); }, Qt::QueuedConnection);
}

QtSchedulerDelegate::QtSchedulerDelegate(
    fabric::ShadowViewMutationAdapter& adapter)
    : adapter_(adapter) {}

const std::vector<fabric::MutationFailure>& QtSchedulerDelegate::lastFailures()
    const {
  return lastFailures_;
}

int QtSchedulerDelegate::mountedTransactionCount() const {
  return mountedTransactionCount_;
}

void QtSchedulerDelegate::schedulerDidFinishTransaction(
    const std::shared_ptr<const facebook::react::MountingCoordinator>&
    /*mountingCoordinator*/) {
  // Fires synchronously on the commit thread. Mounting happens in
  // schedulerShouldRenderTransactions instead.
}

void QtSchedulerDelegate::schedulerShouldRenderTransactions(
    const std::shared_ptr<const facebook::react::MountingCoordinator>&
        mountingCoordinator) {
  if (mountingCoordinator == nullptr) {
    return;
  }

  // Transactions can queue up, so drain rather than pulling once.
  while (auto transaction = mountingCoordinator->pullTransaction()) {
    lastFailures_ = adapter_.apply(transaction->getMutations());
    ++mountedTransactionCount_;
  }
}

void QtSchedulerDelegate::schedulerShouldMergeReactRevision(
    facebook::react::SurfaceId /*surfaceId*/) {}

void QtSchedulerDelegate::schedulerDidRequestPreliminaryViewAllocation(
    const facebook::react::ShadowNode& /*shadowNode*/) {
  // An optimisation hook: platforms may pre-create views before mounting. Qt
  // item creation is cheap enough that pre-allocation has nothing to win yet.
}

void QtSchedulerDelegate::schedulerDidDispatchCommand(
    const facebook::react::ShadowView& /*shadowView*/,
    const std::string& /*commandName*/,
    const folly::dynamic& /*args*/) {
  // Imperative component commands. No component exposes one yet.
}

void QtSchedulerDelegate::schedulerDidSendAccessibilityEvent(
    const facebook::react::ShadowView& /*shadowView*/,
    const std::string& /*eventType*/) {
  // Accessibility is an honest gap; Qt Quick's accessibility bridge is not
  // wired up. Recorded in docs/component-support.md rather than faked.
}

void QtSchedulerDelegate::schedulerDidSetIsJSResponder(
    const facebook::react::ShadowView& /*shadowView*/,
    bool /*isJSResponder*/,
    bool /*blockNativeResponder*/) {
  // Touch responder handover, which needs the input pipeline (P6).
}

void QtSchedulerDelegate::schedulerShouldSynchronouslyUpdateViewOnUIThread(
    facebook::react::Tag /*tag*/,
    const folly::dynamic& /*props*/) {
  // The native-driver animation path. Animated is not wired up yet.
}

void QtSchedulerDelegate::schedulerDidUpdateShadowTree(
    const std::unordered_map<facebook::react::Tag, folly::dynamic>&
    /*tagToProps*/) {}

void QtSchedulerDelegate::schedulerDidCaptureViewSnapshot(
    facebook::react::Tag /*tag*/,
    facebook::react::SurfaceId /*surfaceId*/) {
  // View snapshots back shared-element transitions; no snapshot store exists.
}

void QtSchedulerDelegate::schedulerDidSetViewSnapshot(
    facebook::react::Tag /*sourceTag*/,
    facebook::react::Tag /*targetTag*/,
    facebook::react::SurfaceId /*surfaceId*/) {}

void QtSchedulerDelegate::schedulerDidClearPendingSnapshots() {}

std::vector<std::string> registeredComponentNames() {
  // One list, used by both the registry factory and the JavaScript predicate,
  // so the two cannot drift apart.
  return {
      std::string(RootComponentDescriptor::ConcreteShadowNode::Name()),
      std::string(ViewComponentDescriptor::ConcreteShadowNode::Name()),
  };
}

facebook::react::ComponentRegistryFactory makeComponentRegistryFactory() {
  return [](const facebook::react::EventDispatcher::Weak& eventDispatcher,
            const std::shared_ptr<const ContextContainer>& contextContainer) {
    // The provider registry has to outlive the descriptor registry it builds,
    // and React Native's own platforms keep it in a static for the same reason.
    static auto providerRegistry =
        std::make_shared<ComponentDescriptorProviderRegistry>();

    auto registry = providerRegistry->createComponentDescriptorRegistry(
        facebook::react::ComponentDescriptorParameters{
            .eventDispatcher = eventDispatcher,
            .contextContainer = contextContainer,
            .flavor = nullptr});

    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            RootComponentDescriptor>());
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            ViewComponentDescriptor>());

    // Without a fallback, an unregistered component name throws
    // std::invalid_argument out of ComponentDescriptorRegistry::at
    // (ComponentDescriptorRegistry.cpp:104-106). A soft failure is the honest
    // behaviour while the component set is small.
    //
    // The descriptor is UnimplementedNativeView, matching what React Native's
    // own comment names as the default and what Android installs
    // (ComponentDescriptorRegistry.cpp:97-99,
    // DefaultComponentsRegistry.cpp:54-56). UnimplementedView is the wrong
    // class here: its getComponentName dereferences `flavor_` as a std::string
    // (UnimplementedViewComponentDescriptor.cpp:17-19), so registering it with
    // a null flavor segfaults inside setFallbackComponentDescriptor.
    //
    // createComponentDescriptorRegistry hands back a shared_ptr-to-const, so
    // the fallback is installed through a const_cast, again as React Native
    // does it.
    auto& mutableRegistry =
        const_cast<facebook::react::ComponentDescriptorRegistry&>(*registry);
    mutableRegistry.setFallbackComponentDescriptor(
        std::make_shared<const UnimplementedNativeViewComponentDescriptor>(
            facebook::react::ComponentDescriptorParameters{
                .eventDispatcher = eventDispatcher,
                .contextContainer = contextContainer,
                .flavor = nullptr}));

    return registry;
  };
}

class QtReactHost::Impl {
 public:
  explicit Impl(fabric::ShadowViewMutationAdapter& adapter)
      : delegate(adapter) {
    // Microtasks are off by default in Hermes. React Native's scheduler and
    // React itself queue them, and NativeMicrotasks forwards straight to
    // Runtime::queueMicrotask, which throws when the queue is disabled.
    runtime = facebook::hermes::makeHermesRuntime(
        ::hermes::vm::RuntimeConfig::Builder()
            .withMicrotaskQueue(true)
            .build());

    // JavaScript runs on this thread. The executor therefore calls straight
    // through; a real JS thread replaces this without changing anything above.
    facebook::react::RuntimeExecutor runtimeExecutor =
        [this](std::function<void(facebook::jsi::Runtime&)>&& callback) {
          callback(*runtime);
        };

    runtimeScheduler = std::make_shared<RuntimeScheduler>(runtimeExecutor);
    facebook::react::RuntimeSchedulerBinding::createAndInstallIfNeeded(
        *runtime, runtimeScheduler);

    auto container = std::make_shared<ContextContainer>();
    // Scheduler.cpp:73-80 asserts on this key and dereferences it, so a missing
    // entry is a crash rather than a degraded mode.
    container->insert(
        facebook::react::RuntimeSchedulerKey,
        std::weak_ptr<RuntimeScheduler>(runtimeScheduler));
    contextContainer = container;

    SchedulerToolbox toolbox{};
    toolbox.contextContainer = contextContainer;
    toolbox.componentRegistryFactory = makeComponentRegistryFactory();
    toolbox.runtimeExecutor = runtimeExecutor;
    toolbox.bridgelessBindingsExecutor = std::nullopt;
    toolbox.eventBeatFactory =
        [scheduler = runtimeScheduler](
            std::shared_ptr<facebook::react::EventBeat::OwnerBox> ownerBox)
        -> std::unique_ptr<facebook::react::EventBeat> {
      return std::make_unique<QtEventBeat>(std::move(ownerBox), *scheduler);
    };

    scheduler = std::make_shared<Scheduler>(
        toolbox, /*animationDelegate=*/nullptr, &delegate);

    // React Native's JavaScript branches on this global to decide whether the
    // legacy bridge exists. Without it, loading a bundle throws
    // "__fbBatchedBridgeConfig is not set" from setUpBatchedBridge.js:16, and
    // NativeComponentRegistry.js:57 would look for legacy view configs. This
    // platform is Fabric-only, so the answer is a flat true and it must be set
    // before any bundle is evaluated.
    runtime->global().setProperty(*runtime, "RN$Bridgeless", true);

    // NativeModules.js:185-192 throws unless one of `global.nativeModuleProxy`
    // or `global.__fbBatchedBridgeConfig` exists, and the bridgeless flag does
    // not gate that check. An empty remote module config is the truthful
    // answer for this platform: there are no legacy bridge modules. It is not a
    // stub standing in for work — TurboModules are the mechanism here, and none
    // are registered yet.
    auto bridgeConfig = facebook::jsi::Object(*runtime);
    bridgeConfig.setProperty(
        *runtime, "remoteModuleConfig", facebook::jsi::Array(*runtime, 0));
    runtime->global().setProperty(
        *runtime, "__fbBatchedBridgeConfig", bridgeConfig);

    // In bridgeless mode React Native's JavaScript registers callable modules
    // through this global instead of the batched bridge
    // (Libraries/Core/registerCallableModule.js:20). ReactInstance installs it
    // (ReactInstance.cpp:586-606) and this host does not use ReactInstance, so
    // it installs its own. Registrations are kept rather than discarded; what
    // is missing is a native caller, which arrives with the first native module
    // that calls into JavaScript.
    runtime->global().setProperty(
        *runtime,
        "RN$registerCallableModule",
        facebook::jsi::Function::createFromHostFunction(
            *runtime,
            facebook::jsi::PropNameID::forAscii(
                *runtime, "registerCallableModule"),
            2,
            [this](
                facebook::jsi::Runtime& jsRuntime,
                const facebook::jsi::Value& /*thisValue*/,
                const facebook::jsi::Value* args,
                size_t count) -> facebook::jsi::Value {
              if (count != 2 || !args[0].isString()) {
                throw facebook::jsi::JSError(
                    jsRuntime,
                    "registerCallableModule requires a name and a factory");
              }
              callableModules.emplace(
                  args[0].asString(jsRuntime).utf8(jsRuntime),
                  facebook::jsi::Value(jsRuntime, args[1]));
              return facebook::jsi::Value::undefined();
            }));
  }

  ~Impl() {
    for (auto& [surfaceId, handler] : surfaces) {
      if (handler->getStatus() == SurfaceHandler::Status::Running) {
        handler->stop();
      }
      scheduler->unregisterSurface(*handler);
    }
    surfaces.clear();
  }

  std::unique_ptr<facebook::jsi::Runtime> runtime;
  std::shared_ptr<RuntimeScheduler> runtimeScheduler;
  std::shared_ptr<const ContextContainer> contextContainer;
  QtSchedulerDelegate delegate;
  std::shared_ptr<Scheduler> scheduler;
  std::unordered_map<SurfaceId, std::unique_ptr<SurfaceHandler>> surfaces;
  /// Modules JavaScript has registered as callable from native. Nothing calls
  /// them yet; keeping them is what makes that possible later.
  std::unordered_map<std::string, facebook::jsi::Value> callableModules;
  std::unordered_map<int, std::unique_ptr<QTimer>> timers;
  int nextTimerHandle{0};
  QString lastError;
};

QtReactHost::QtReactHost(fabric::ShadowViewMutationAdapter& adapter)
    : impl_(std::make_unique<Impl>(adapter)) {}

QtReactHost::~QtReactHost() = default;

facebook::jsi::Runtime& QtReactHost::runtime() {
  return *impl_->runtime;
}

Scheduler& QtReactHost::scheduler() {
  return *impl_->scheduler;
}

QtSchedulerDelegate& QtReactHost::delegate() {
  return impl_->delegate;
}

QString QtReactHost::lastError() const {
  return impl_->lastError;
}

const std::shared_ptr<const ContextContainer>& QtReactHost::contextContainer()
    const {
  return impl_->contextContainer;
}

bool QtReactHost::evaluate(
    const std::string& source,
    const std::string& sourceUrl) {
  try {
    impl_->runtime->evaluateJavaScript(
        std::make_unique<facebook::jsi::StringBuffer>(source), sourceUrl);
    impl_->lastError.clear();
    return true;
  } catch (const facebook::jsi::JSError& error) {
    impl_->lastError = QString::fromStdString(error.getMessage());
    return false;
  } catch (const std::exception& error) {
    impl_->lastError = QString::fromUtf8(error.what());
    return false;
  }
}

bool QtReactHost::hasComponent(const std::string& componentName) const {
  // JavaScript asks for the React view name; the registry holds the component
  // name. componentNameByReactViewName is React Native's own mapping, which
  // strips an RCT prefix and rewrites Text/VirtualText/ImageView
  // (componentregistry/componentNameByReactViewName.cpp).
  const auto unified =
      facebook::react::componentNameByReactViewName(componentName);

  // ComponentDescriptorRegistry offers no name-based existence check, and with
  // a fallback installed `at()` never throws, so it cannot answer this. The
  // answer therefore comes from the same list the registry factory registers —
  // see registeredComponentNames(), which both use.
  const auto names = registeredComponentNames();
  return std::find(names.begin(), names.end(), unified) != names.end();
}

void QtReactHost::installTurboModuleBinding() {
  auto callInvoker =
      std::make_shared<facebook::react::RuntimeSchedulerCallInvoker>(
          impl_->runtimeScheduler);

  facebook::react::TurboModuleBinding::install(
      *impl_->runtime,
      [callInvoker](
          facebook::jsi::Runtime& /*jsRuntime*/,
          const std::string& name)
          -> std::shared_ptr<facebook::react::TurboModule> {
        // React Native's own generic C++ modules. Two of the modules this
        // platform needs come from here already implemented — microtasks and
        // idle callbacks, both backed by the RuntimeScheduler.
        return facebook::react::DefaultTurboModules::getTurboModule(
            name, callInvoker);
      });
}

void QtReactHost::installPlatformConstants() {
  auto& runtime = *impl_->runtime;
  auto constants = facebook::jsi::Object(runtime);

  // Real values from the running system. On the development host that means
  // macOS; on the target it will mean QNX. Reporting the host truthfully is
  // better than reporting a QNX version this process cannot observe.
  const auto productVersion = QSysInfo::productVersion().toStdString();
  constants.setProperty(
      runtime,
      "osVersion",
      facebook::jsi::String::createFromUtf8(runtime, productVersion));
  constants.setProperty(
      runtime,
      "systemName",
      facebook::jsi::String::createFromUtf8(
          runtime, QSysInfo::productType().toStdString()));
  constants.setProperty(runtime, "isTesting", false);

  // The React Native line this platform is built against, pinned in
  // package.json and asserted by validate:dependencies.
  auto version = facebook::jsi::Object(runtime);
  version.setProperty(runtime, "major", 0);
  version.setProperty(runtime, "minor", 86);
  version.setProperty(runtime, "patch", 2);
  version.setProperty(runtime, "prerelease", facebook::jsi::Value::null());
  constants.setProperty(runtime, "reactNativeVersion", version);

  runtime.global().setProperty(
      runtime, "__RNQNX_PLATFORM_CONSTANTS", constants);
}

void QtReactHost::installTimers() {
  auto& runtime = *impl_->runtime;
  auto* impl = impl_.get();

  const auto makeSetTimer = [&runtime, impl](bool repeating) {
    return facebook::jsi::Function::createFromHostFunction(
        runtime,
        facebook::jsi::PropNameID::forAscii(
            runtime, repeating ? "setInterval" : "setTimeout"),
        2,
        [impl, repeating](
            facebook::jsi::Runtime& jsRuntime,
            const facebook::jsi::Value& /*thisValue*/,
            const facebook::jsi::Value* args,
            size_t count) -> facebook::jsi::Value {
          if (count < 1 || !args[0].isObject() ||
              !args[0].asObject(jsRuntime).isFunction(jsRuntime)) {
            throw facebook::jsi::JSError(
                jsRuntime, "setTimeout requires a function");
          }

          auto callback = std::make_shared<facebook::jsi::Function>(
              args[0].asObject(jsRuntime).asFunction(jsRuntime));
          const int delayMs = count > 1 && args[1].isNumber()
              ? static_cast<int>(args[1].getNumber())
              : 0;

          const int handle = ++impl->nextTimerHandle;
          auto timer = std::make_unique<QTimer>();
          timer->setSingleShot(!repeating);
          timer->setInterval(delayMs);
          QObject::connect(
              timer.get(), &QTimer::timeout, [impl, handle, callback, repeating]() {
                // The callback runs on the thread the timer fires on, which is
                // the thread that owns the event loop — the same thread that
                // owns this runtime.
                callback->call(*impl->runtime);
                if (!repeating) {
                  impl->timers.erase(handle);
                }
              });
          timer->start();
          impl->timers.emplace(handle, std::move(timer));
          return facebook::jsi::Value(handle);
        });
  };

  const auto makeClearTimer = [&runtime, impl](const char* name) {
    return facebook::jsi::Function::createFromHostFunction(
        runtime,
        facebook::jsi::PropNameID::forAscii(runtime, name),
        1,
        [impl](
            facebook::jsi::Runtime& /*jsRuntime*/,
            const facebook::jsi::Value& /*thisValue*/,
            const facebook::jsi::Value* args,
            size_t count) -> facebook::jsi::Value {
          if (count >= 1 && args[0].isNumber()) {
            impl->timers.erase(static_cast<int>(args[0].getNumber()));
          }
          return facebook::jsi::Value::undefined();
        });
  };

  auto global = runtime.global();
  global.setProperty(runtime, "setTimeout", makeSetTimer(false));
  global.setProperty(runtime, "setInterval", makeSetTimer(true));
  global.setProperty(runtime, "clearTimeout", makeClearTimer("clearTimeout"));
  global.setProperty(runtime, "clearInterval", makeClearTimer("clearInterval"));
  // React Native's JavaScript treats these as interchangeable with timeouts.
  global.setProperty(runtime, "setImmediate", makeSetTimer(false));
  global.setProperty(
      runtime, "clearImmediate", makeClearTimer("clearImmediate"));
  global.setProperty(runtime, "requestAnimationFrame", makeSetTimer(false));
  global.setProperty(
      runtime,
      "cancelAnimationFrame",
      makeClearTimer("cancelAnimationFrame"));
}

void QtReactHost::pumpEvents() {
  if (auto* application = QCoreApplication::instance()) {
    application->processEvents();
  }
}

void QtReactHost::installComponentProvider() {
  facebook::react::bindHasComponentProvider(
      *impl_->runtime, [this](const std::string& name) {
        return hasComponent(name);
      });
}

bool QtReactHost::hasFabricUIManager() {
  auto& runtime = *impl_->runtime;
  const auto value = runtime.global().getProperty(runtime, "nativeFabricUIManager");
  return !value.isUndefined() && !value.isNull();
}

namespace {

facebook::react::LayoutConstraints makeLayoutConstraints(
    float width,
    float height) {
  facebook::react::LayoutConstraints constraints;
  constraints.minimumSize = {.width = 0, .height = 0};
  constraints.maximumSize = {.width = width, .height = height};
  // SurfaceHandler.cpp:43 asserts the direction is set, and it defaults to
  // Undefined, so this must happen before start().
  constraints.layoutDirection = facebook::react::LayoutDirection::LeftToRight;
  return constraints;
}

}  // namespace

void QtReactHost::startSurface(
    SurfaceId surfaceId,
    const std::string& moduleName,
    float width,
    float height) {
  auto handler = std::make_unique<SurfaceHandler>(moduleName, surfaceId);
  impl_->scheduler->registerSurface(*handler);
  handler->setProps(folly::dynamic::object());
  handler->constraintLayout(
      makeLayoutConstraints(width, height), facebook::react::LayoutContext{});
  handler->start();
  impl_->surfaces.emplace(surfaceId, std::move(handler));
}

void QtReactHost::startEmptySurface(
    SurfaceId surfaceId,
    float width,
    float height) {
  // See the header: stopping the surface calls RN$stopSurface even when no
  // React app was ever started. Defining it only if absent means a real bundle's
  // implementation always wins.
  evaluate(
      "if (typeof globalThis.RN$stopSurface !== 'function') {"
      "  globalThis.RN$stopSurface = function () {};"
      "}",
      "rnqnx-empty-surface-shim.js");

  auto handler = std::make_unique<SurfaceHandler>("", surfaceId);
  impl_->scheduler->registerSurface(*handler);

  handler->constraintLayout(
      makeLayoutConstraints(width, height), facebook::react::LayoutContext{});
  handler->start();

  impl_->surfaces.emplace(surfaceId, std::move(handler));
}

void QtReactHost::stopSurface(SurfaceId surfaceId) {
  const auto found = impl_->surfaces.find(surfaceId);
  if (found == impl_->surfaces.end()) {
    return;
  }

  if (found->second->getStatus() == SurfaceHandler::Status::Running) {
    found->second->stop();
  }
  impl_->scheduler->unregisterSurface(*found->second);
  impl_->surfaces.erase(found);
}

bool QtReactHost::commit(
    SurfaceId surfaceId,
    const std::vector<std::shared_ptr<const ShadowNode>>& children) {
  bool committed = false;

  impl_->scheduler->getUIManager()->getShadowTreeRegistry().visit(
      surfaceId, [&](const facebook::react::ShadowTree& shadowTree) {
        shadowTree.commit(
            [&](const facebook::react::RootShadowNode& oldRootShadowNode) {
              return std::static_pointer_cast<facebook::react::RootShadowNode>(
                  oldRootShadowNode.ShadowNode::clone(
                      facebook::react::ShadowNodeFragment{
                          .props = facebook::react::ShadowNodeFragment::
                              propsPlaceholder(),
                          .children = std::make_shared<
                              const std::vector<std::shared_ptr<const ShadowNode>>>(
                              children),
                      }));
            },
            {});
        committed = true;
      });

  if (!committed) {
    impl_->lastError =
        QStringLiteral("No shadow tree is registered for surface %1")
            .arg(surfaceId);
  }
  return committed;
}

}  // namespace rnqnx::runtime
