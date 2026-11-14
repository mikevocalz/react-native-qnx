#include "react-native-qnx/runtime/QtReactHost.h"

#include <hermes/hermes.h>
#include <hermes/Public/RuntimeConfig.h>

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/componentregistry/componentNameByReactViewName.h>
#include <react/renderer/componentregistry/native/NativeComponentRegistryBinding.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/FBReactNativeSpec/ComponentDescriptors.h>
#include <react/renderer/components/image/ImageComponentDescriptor.h>
#include <react/renderer/components/scrollview/ScrollViewComponentDescriptor.h>
#include <react/renderer/components/iostextinput/TextInputComponentDescriptor.h>
#include <react/renderer/components/switch/iosswitch/react/renderer/components/switch/AppleSwitchComponentDescriptor.h>
#include <react/renderer/components/safeareaview/SafeAreaViewComponentDescriptor.h>
#include <react/renderer/components/modal/ModalHostViewComponentDescriptor.h>
#include <react/renderer/components/text/ParagraphComponentDescriptor.h>
#include <react/renderer/components/text/RawTextComponentDescriptor.h>
#include <react/renderer/components/text/TextComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>
#include "react-native-qnx/fabric/QtExtraComponents.h"
#if RNQNX_ENABLE_QT_SVG
#include <react/renderer/components/RnqnxQtSvg/ComponentDescriptors.h>
#endif
#include <react/renderer/imagemanager/ImageManager.h>
#include <react/renderer/textlayoutmanager/TextLayoutManager.h>
#include <react/renderer/core/ShadowNodeFragment.h>
#include <react/renderer/mounting/MountingCoordinator.h>
#include <react/renderer/mounting/MountingTransaction.h>
#include <react/renderer/mounting/ShadowTree.h>
#include <react/renderer/runtimescheduler/RuntimeSchedulerBinding.h>
#include <react/renderer/runtimescheduler/RuntimeSchedulerCallInvoker.h>
#include <jserrorhandler/ErrorUtils.h>
#include <ReactCommon/TurboModuleBinding.h>
#include <react/nativemodule/defaults/DefaultTurboModules.h>
#include <react/renderer/scheduler/SchedulerToolbox.h>
#include <react/renderer/uimanager/UIManager.h>
#include <react/featureflags/ReactNativeFeatureFlags.h>
#include <react/featureflags/ReactNativeFeatureFlagsDefaults.h>
#include <react/renderer/animated/AnimatedModule.h>
#include <react/renderer/animated/NativeAnimatedNodesManagerProvider.h>
#include "react-native-qnx/runtime/QnxFileReaderModule.h"
#if RNQNX_ENABLE_APP_STATE
#include "react-native-qnx/runtime/QnxAppStateModule.h"
#endif
#include "react-native-qnx/runtime/QnxDevSupportModules.h"
#include "react-native-qnx/runtime/QnxImageLoader.h"
#include "react-native-qnx/runtime/QnxSegmentFetcher.h"
#include "react-native-qnx/runtime/QnxWebSocketModule.h"

#include <folly/dynamic.h>
#include <jsi/JSIDynamic.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QFileInfo>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QMetaObject>

#include <algorithm>
#include <cstdio>
#include <exception>
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

  if (QCoreApplication::instance() == nullptr) {
    // Without an event loop there is nothing to schedule against. Inducing
    // immediately keeps a headless test moving instead of hanging.
    induce();
    return;
  }

  // Ask the Qt event loop for the tick. Queuing means the beat lands after the
  // current event finishes, which is what the Android and Apple beats do with
  // their own frame callbacks.
  //
  // The receiver is this beat's own QObject rather than
  // QCoreApplication::instance(). A queued call outlives the moment it was
  // posted and the application object outlives every host, so posting there
  // left a pending `induce()` on a destroyed beat whenever a host was torn
  // down with a beat in flight — a segfault in whatever pumped the loop next.
  // ~QObject drops that object's posted events, so the stale beat is discarded
  // instead. React Native's OwnerBox does not help: it is checked inside
  // induce(), which is already too late for a dangling `this`
  // (EventBeat.cpp:62-66).
  QMetaObject::invokeMethod(
      context_.get(), [this]() { induce(); }, Qt::QueuedConnection);
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
    // A failed mutation leaves a view (and often its subtree) off screen with
    // no other trace, so each one is logged.
    for (const auto& failure : lastFailures_) {
      std::fprintf(stderr, "RNQNX_MOUNT_FAILURE type=%d tag=%d error=%d %s\n",
                   static_cast<int>(failure.type), static_cast<int>(failure.tag),
                   static_cast<int>(failure.error), failure.detail.toUtf8().constData());
    }
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
    const facebook::react::ShadowView& shadowView,
    const std::string& commandName,
    const folly::dynamic& args) {
  const auto failure = adapter_.dispatchCommand(
      shadowView.tag, commandName, args, &shadowView);
  if (failure.error != rnqnx::fabric::MountError::None) {
    // Keep the same shape as mount failures so tests and logs can read them.
    lastFailures_.push_back(failure);
  }
}

void QtSchedulerDelegate::schedulerDidSendAccessibilityEvent(
    const facebook::react::ShadowView& /*shadowView*/,
    const std::string& /*eventType*/) {
  // Accessibility is an honest gap; Qt Quick's accessibility bridge is not
  // wired up. Recorded in docs/component-support.md rather than faked.
}

void QtSchedulerDelegate::setTouchDispatcher(
    fabric::QtTouchDispatcher* dispatcher) {
  touchDispatcher_ = dispatcher;
}

void QtSchedulerDelegate::schedulerDidSetIsJSResponder(
    const facebook::react::ShadowView& shadowView,
    bool isJSResponder,
    bool /*blockNativeResponder*/) {
  // blockNativeResponder is ignored: there is no native gesture recogniser to
  // block on this platform yet, so honouring it would change nothing.
  if (touchDispatcher_ != nullptr) {
    touchDispatcher_->setJsResponder(shadowView.tag, isJSResponder);
  }
}

void QtSchedulerDelegate::schedulerShouldSynchronouslyUpdateViewOnUIThread(
    facebook::react::Tag tag,
    const folly::dynamic& props) {
  // The native-driver animation path: apply the prop delta directly to the
  // mounted Qt Quick item without a full shadow-tree commit.
  adapter_.synchronouslyUpdateViewOnUIThread(tag, props);
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
  std::vector<std::string> names = {
      std::string(RootComponentDescriptor::ConcreteShadowNode::Name()),
#if RNQNX_ENABLE_QT_SVG
      std::string(facebook::react::QnxSvgDocumentComponentDescriptor::ConcreteShadowNode::Name()),
#endif
      std::string(ViewComponentDescriptor::ConcreteShadowNode::Name()),
      std::string(facebook::react::ParagraphComponentDescriptor::
                      ConcreteShadowNode::Name()),
      std::string(
          facebook::react::TextComponentDescriptor::ConcreteShadowNode::Name()),
      std::string(facebook::react::RawTextComponentDescriptor::
                      ConcreteShadowNode::Name()),
      std::string(
          facebook::react::ImageComponentDescriptor::ConcreteShadowNode::Name()),
      std::string(facebook::react::ScrollViewComponentDescriptor::
                      ConcreteShadowNode::Name()),
      std::string(facebook::react::TextInputComponentDescriptor::
                      ConcreteShadowNode::Name()),
      std::string(
          facebook::react::ActivityIndicatorViewComponentDescriptor::
              ConcreteShadowNode::Name()),
      std::string(facebook::react::SwitchComponentDescriptor::
                      ConcreteShadowNode::Name()),
      std::string(facebook::react::SafeAreaViewComponentDescriptor::
                      ConcreteShadowNode::Name()),
      std::string(facebook::react::ModalHostViewComponentDescriptor::
                      ConcreteShadowNode::Name()),
  };
  for (auto& name : rnqnx::fabric::extraComponentNames()) names.push_back(std::move(name));
  return names;
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
#if RNQNX_ENABLE_QT_SVG
    providerRegistry->add(facebook::react::concreteComponentDescriptorProvider<
        facebook::react::QnxSvgDocumentComponentDescriptor>());
#endif
    rnqnx::fabric::addExtraComponentDescriptors(*providerRegistry);
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            ViewComponentDescriptor>());
    // Text is three descriptors: Paragraph is the laid-out block, Text is a
    // nested span, and RawText is the string itself.
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::ParagraphComponentDescriptor>());
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::TextComponentDescriptor>());
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::RawTextComponentDescriptor>());
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::ImageComponentDescriptor>());
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::ScrollViewComponentDescriptor>());

    // React Native's textinput descriptor lives under platform/ios, but it is
    // portable C++: TextInputShadowNode and TextInputProps depend only on
    // react/, Yoga and the standard library. The Android one is not an option —
    // it reaches for fbjni and the Java FabricUIManager to read theme padding
    // (AndroidTextInputComponentDescriptor.h:41-47).
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::TextInputComponentDescriptor>());
    // codegen'd FBReactNativeSpec component; ActivityIndicatorView is the
    // Fabric name for RCTActivityIndicatorView (componentNameByReactViewName
    // leaves the non-RCT-prefixed name unchanged).
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::ActivityIndicatorViewComponentDescriptor>());
    // iOS switch descriptor has no Apple dependency in its shadow node and
    // props, but its .mm implementation is not built here. A QNX
    // SwitchShadowNode.cpp provides the component name and a default intrinsic
    // size.
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::SwitchComponentDescriptor>());
    // SafeAreaView has no platform fork; insets are zero on QNX, so it
    // behaves like a View.
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::SafeAreaViewComponentDescriptor>());
    // Modal is portable; on QNX it renders as a full-screen overlay item.
    providerRegistry->add(
        facebook::react::concreteComponentDescriptorProvider<
            facebook::react::ModalHostViewComponentDescriptor>());

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

namespace {
class QnxFeatureFlags : public facebook::react::ReactNativeFeatureFlagsDefaults {
 public:
  bool cxxNativeAnimatedEnabled() override {
    return true;
  }
  // The shared backend requires a UIManagerAnimationDelegate that this host
  // does not yet set, so keep the per-module C++ Animated implementation.
  bool useSharedAnimatedBackend() override {
    return false;
  }
};
} // namespace

class QtReactHost::Impl {
 public:
  explicit Impl(fabric::ShadowViewMutationAdapter& adapter
#if RNQNX_ENABLE_FRAME_TRACE
                , std::shared_ptr<RuntimeTrace> traceSession = nullptr
#endif
                )
      : delegate(adapter) {
#if RNQNX_ENABLE_FRAME_TRACE
    trace = std::move(traceSession);
#endif
    // TouchableOpacity and Button are driven by the native Animated driver.
    // Enabling the per-module C++ backend exposes AnimatedModule without
    // requiring a platform-specific implementation.
    static std::once_flag featureFlagOverride;
    std::call_once(featureFlagOverride, []() {
      facebook::react::ReactNativeFeatureFlags::override(
          std::make_unique<QnxFeatureFlags>());
    });

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
          ++runtimeExecutionDepth;
          try {
            callback(*runtime);
          } catch (...) {
            --runtimeExecutionDepth;
            throw;
          }
          --runtimeExecutionDepth;
        };

    // RN's default error handler calls JS ErrorUtils and may throw if a
    // bootstrap bundle has not installed it. scheduleWork is noexcept, so
    // checkpoint/timer errors need a host-owned reporting boundary instead.
    runtimeScheduler = std::make_shared<RuntimeScheduler>(
        runtimeExecutor,
        facebook::react::HighResTimeStamp::now,
        [this](facebook::jsi::Runtime& runtime, facebook::jsi::JSError& error) {
          ++taskErrorSequence;
          const auto previousReports = javaScriptErrors.size();
          // Preserve an installed JS error hook, but never let a missing or
          // broken ErrorUtils handler escape through scheduleWork's noexcept.
          try {
            const auto utils = runtime.global().getProperty(runtime, "ErrorUtils");
            if (utils.isObject()) {
              const auto object = utils.asObject(runtime);
              if (object.hasProperty(runtime, "reportFatalError") &&
                  object.hasProperty(runtime, "reportError")) {
                facebook::react::handleJSError(runtime, error, true);
              }
            }
          } catch (...) {
            qWarning() << "ErrorUtils reporting failed; retaining the host error";
          }
          if (javaScriptErrors.size() == previousReports) {
            javaScriptErrors.push_back(
                {error.getMessage(), error.getStack(), true});
          }
          lastError = QString::fromStdString(error.getMessage());
          qWarning().noquote() << "[js task error]" << lastError;
        });
    facebook::react::RuntimeSchedulerBinding::createAndInstallIfNeeded(
        *runtime, runtimeScheduler);

    taskQueue = std::make_shared<QtRuntimeTaskQueue>(
        [scheduler = std::weak_ptr<RuntimeScheduler>(runtimeScheduler)](
            QtRuntimeTaskQueue::Task task) {
          if (auto owner = scheduler.lock()) {
            owner->scheduleWork(
                [task = std::move(task)](facebook::jsi::Runtime&) { task(); });
          }
        },
        [this]() { ++runtimeExecutionDepth; },
        [this]() {
          --runtimeExecutionDepth;
          if (runtimeExecutionDepth == 0) {
            runtimeScheduler->scheduleWork([](facebook::jsi::Runtime&) {});
          }
        }
#if RNQNX_ENABLE_FRAME_TRACE
        , trace
#endif
        );

    auto container = std::make_shared<ContextContainer>();
    // Scheduler.cpp:73-80 asserts on this key, and dereferences it, so a missing
    // entry is a crash rather than a degraded mode.
    container->insert(
        facebook::react::RuntimeSchedulerKey,
        std::weak_ptr<RuntimeScheduler>(runtimeScheduler));

    // BaseParagraphComponentDescriptor.h:23 pulls the TextLayoutManager out of
    // the context container by name; without it, constructing a Paragraph
    // descriptor throws.
    container->insert(
        "TextLayoutManager",
        std::make_shared<const facebook::react::TextLayoutManager>(nullptr));

    // ImageComponentDescriptor pulls this out by name the same way. React
    // Native's generic C++ ImageManager is a documented stub — it does not
    // fetch or decode — which is correct here: Qt Quick's Image item loads from
    // the URI itself, so the manager has nothing to do on this platform.
    container->insert(
        "ImageManager",
        std::make_shared<facebook::react::ImageManager>(nullptr));
    contextContainer = container;

    SchedulerToolbox toolbox{};
    toolbox.contextContainer = contextContainer;
    toolbox.componentRegistryFactory = makeComponentRegistryFactory();
    // UIManager uses this executor for AppRegistry start/update/stop. Those
    // calls must also enter scheduler tasks; invoking React directly can leave
    // its initial callbackNode stale before a later useState update.
    toolbox.runtimeExecutor =
        [scheduler = std::weak_ptr<RuntimeScheduler>(runtimeScheduler)](
            std::function<void(facebook::jsi::Runtime&)>&& callback) {
          if (auto runtimeScheduler = scheduler.lock()) {
            runtimeScheduler->scheduleWork(std::move(callback));
          }
        };
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
#if RNQNX_ENABLE_TEXT_INPUT_CONTRACT
    runtime->global().setProperty(*runtime, "__RNQNX_TEXT_INPUT_CONTRACT__", true);
#endif
#if defined(RNQNX_ENABLE_INPUT_LIFECYCLE) && RNQNX_ENABLE_INPUT_LIFECYCLE
    // Pinned RN's EventTarget dispatch skips null targets. Opt-in cancellation
    // must still retire a responder after its React element was unmounted.
    runtime->global().setProperty(*runtime, "__RNQNX_INPUT_LIFECYCLE__", true);
#endif

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
    if (runtimeExecutionDepth != 0) {
      std::terminate();
    }
    views::ImageLoadedNotifier::shared().unsubscribe(imageLoadedSubscription);
    for (auto& [surfaceId, handler] : surfaces) {
#if RNQNX_ENABLE_FRAME_TRACE
      const auto generation = surfaceGenerations.find(surfaceId);
      TraceSpan stopTrace(trace, TraceStage::SurfaceStop, surfaceId,
          generation == surfaceGenerations.end() ? 0 : generation->second);
#endif
      if (handler->getStatus() == SurfaceHandler::Status::Running) {
        handler->stop();
      }
      scheduler->unregisterSurface(*handler);
    }
    surfaces.clear();
    // React unmount cleanup may still invoke native callbacks or stop producers.
    // Close admission only after those entries, while Hermes/scheduler live.
#if RNQNX_ENABLE_APP_STATE
    // Stop the producer and invalidate pending AppState deliveries while the
    // runtime is still alive. RuntimeScheduler work can then drain safely.
    if (appStateModule) appStateModule->invalidate();
#endif
    taskQueue->close();
  }

#if RNQNX_ENABLE_FRAME_TRACE
  std::shared_ptr<RuntimeTrace> trace;
  std::unordered_map<SurfaceId, std::uint64_t> surfaceGenerations;
#endif
  int runtimeExecutionDepth{0};
  std::size_t taskErrorSequence{0};
  std::unique_ptr<facebook::jsi::Runtime> runtime;
  std::shared_ptr<RuntimeScheduler> runtimeScheduler;
  std::shared_ptr<QtRuntimeTaskQueue> taskQueue;
#if RNQNX_ENABLE_APP_STATE
  std::shared_ptr<QnxAppStateModule> appStateModule;
#endif
  std::shared_ptr<const ContextContainer> contextContainer;
  QtSchedulerDelegate delegate;
  std::shared_ptr<Scheduler> scheduler;
  std::unordered_map<SurfaceId, std::unique_ptr<SurfaceHandler>> surfaces;
  /// Modules JavaScript has registered as callable from native. Nothing calls
  /// them yet; keeping them is what makes that possible later.
  std::unordered_map<std::string, facebook::jsi::Value> callableModules;
  /// Module objects from callableModules factories, resolved on first call.
  std::unordered_map<std::string, facebook::jsi::Object> resolvedCallableModules;
  // The window size last sent to JavaScript (setWindowSize).
  float windowWidth{0};
  float windowHeight{0};
  float windowScale{0};
  /// Extra TurboModules supplied by the embedding application.
  QtReactHost::TurboModuleProvider turboModuleProvider;
  // Native modules own asynchronous operations beyond a temporary JS lookup.
  // Cache one instance per host, as the platform TurboModuleManager does.
  std::unordered_map<std::string, std::shared_ptr<facebook::react::TurboModule>> turboModules;
  views::ImageLoadedNotifier::Token imageLoadedSubscription{0};
  std::unordered_map<int, std::unique_ptr<QTimer>> timers;
  /// Drives the NativeAnimated `onRender` callback while at least one animation
  /// is active. Created on first use and reused for later animations.
  std::unique_ptr<QTimer> animationTimer;
  /// The latest frame callback provided by NativeAnimated.
  std::function<void()> animationFrame;
  std::vector<JavaScriptError> javaScriptErrors;
  /// The source URL of the most recently evaluated script, reported by the
  /// SourceCode module.
  std::string scriptUrl;
  int nextTimerHandle{0};
  QString lastError;
};

QtReactHost::QtReactHost(fabric::ShadowViewMutationAdapter& adapter)
    : impl_(std::make_unique<Impl>(adapter)) {}

#if RNQNX_ENABLE_FRAME_TRACE
QtReactHost::QtReactHost(fabric::ShadowViewMutationAdapter& adapter,
                         std::shared_ptr<RuntimeTrace> trace)
    : impl_(std::make_unique<Impl>(adapter, std::move(trace))) {}
#endif

QtReactHost::~QtReactHost() = default;

facebook::jsi::Runtime& QtReactHost::runtime() {
  return *impl_->runtime;
}

std::shared_ptr<QtRuntimeTaskQueue> QtReactHost::taskQueue() const {
  return impl_->taskQueue;
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
#if RNQNX_ENABLE_FRAME_TRACE
  TraceSpan trace(impl_->trace, TraceStage::Evaluate);
#endif
  // completeRoot schedules rendering for the end of an RN event-loop tick.
  // Evaluating directly in JSI bypasses that tick and can also re-enter React's
  // task scheduling while it is still assigning a callback node. This host's
  // executor is synchronous, so scheduling a task at an idle boundary preserves
  // evaluate's synchronous contract while running microtasks and rendering.
  if (impl_->runtimeExecutionDepth != 0) {
    impl_->lastError = QStringLiteral("Cannot re-enter an active JavaScript task");
    return false;
  }
  // RN reads and caches SourceCode.scriptURL during bundle initialization, so
  // it must be set before evaluation. A local path becomes an absolute file
  // URL; without one, a bundled asset resolves to file://assets/... and never
  // loads. Ported from 083c4bd (v1 integration).
  {
    const auto sourceLocation = QString::fromStdString(sourceUrl);
    impl_->scriptUrl = sourceLocation.isEmpty() || !QUrl(sourceLocation).scheme().isEmpty()
        ? sourceUrl
        : QUrl::fromLocalFile(QFileInfo(sourceLocation).absoluteFilePath())
              .toString(QUrl::FullyEncoded)
              .toStdString();
    if (const auto found = impl_->turboModules.find(QnxSourceCode::kModuleName);
        found != impl_->turboModules.end()) {
      if (auto source = std::dynamic_pointer_cast<QnxSourceCode>(found->second))
        source->setScriptUrl(impl_->scriptUrl);
    }
  }
  try {
    std::exception_ptr evaluationError;
    const auto previousTaskError = impl_->taskErrorSequence;
    impl_->runtimeScheduler->scheduleWork(
        [&](facebook::jsi::Runtime& runtime) {
          try {
            runtime.evaluateJavaScript(
                std::make_unique<facebook::jsi::StringBuffer>(source), sourceUrl);
          } catch (...) {
            evaluationError = std::current_exception();
          }
        });
    if (evaluationError) {
      std::rethrow_exception(evaluationError);
    }
    if (impl_->taskErrorSequence != previousTaskError) {
      // The script itself may succeed while its microtask checkpoint fails.
      // Keep the host error from that checkpoint rather than clearing it.
      return false;
    }
    impl_->lastError.clear();
    return true;
  } catch (const facebook::jsi::JSError& error) {
    impl_->lastError = QString::fromStdString(error.getMessage() + "\n" + error.getStack());
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

const std::vector<JavaScriptError>& QtReactHost::javaScriptErrors() const {
  return impl_->javaScriptErrors;
}

void QtReactHost::setTurboModuleProvider(TurboModuleProvider provider) {
  impl_->turboModuleProvider = std::move(provider);
}

void QtReactHost::callFunctionOnModule(
    const std::string& moduleName,
    const std::string& methodName,
    folly::dynamic args,
    bool ifRegistered) {
  auto* impl = impl_.get();
  impl_->runtimeScheduler->scheduleWork(
      [impl, moduleName, methodName, ifRegistered, args = std::move(args)](
          facebook::jsi::Runtime& runtime) {
        auto resolved = impl->resolvedCallableModules.find(moduleName);
        if (resolved == impl->resolvedCallableModules.end()) {
          const auto registered = impl->callableModules.find(moduleName);
          if (registered == impl->callableModules.end()) {
            if (ifRegistered) return;
            throw facebook::jsi::JSError(
                runtime, "Callable module " + moduleName + " is not registered");
          }
          // registerCallableModule always stores a factory: either React
          // Native's lazy `() => require(...)` or a wrapper returning the
          // object it was given (registerCallableModule.js:20-29).
          auto module = registered->second.asObject(runtime)
                            .asFunction(runtime)
                            .call(runtime)
                            .asObject(runtime);
          resolved = impl->resolvedCallableModules
                         .emplace(moduleName, std::move(module))
                         .first;
        }
        auto method = resolved->second.getProperty(runtime, methodName.c_str());
        if (!method.isObject() || !method.asObject(runtime).isFunction(runtime)) {
          throw facebook::jsi::JSError(
              runtime, moduleName + "." + methodName + " is not a function");
        }
        std::vector<facebook::jsi::Value> values;
        values.reserve(args.size());
        for (const auto& arg : args) {
          values.push_back(facebook::jsi::valueFromDynamic(runtime, arg));
        }
        method.asObject(runtime).asFunction(runtime).callWithThis(
            runtime,
            resolved->second,
            static_cast<const facebook::jsi::Value*>(values.data()),
            values.size());
      });
}

void QtReactHost::installGlobalEvalWithSourceUrl() {
  auto& runtime = *impl_->runtime;
  runtime.global().setProperty(
      runtime,
      "globalEvalWithSourceUrl",
      facebook::jsi::Function::createFromHostFunction(
          runtime,
          facebook::jsi::PropNameID::forAscii(runtime, "globalEvalWithSourceUrl"),
          2,
          [](facebook::jsi::Runtime& jsRuntime,
             const facebook::jsi::Value& /*thisValue*/,
             const facebook::jsi::Value* args,
             size_t count) -> facebook::jsi::Value {
            if (count < 1 || !args[0].isString()) {
              throw facebook::jsi::JSError(
                  jsRuntime, "globalEvalWithSourceUrl requires a source string");
            }
            const std::string url = count > 1 && args[1].isString()
                ? args[1].asString(jsRuntime).utf8(jsRuntime)
                : std::string();
            return jsRuntime.evaluateJavaScript(
                std::make_shared<facebook::jsi::StringBuffer>(
                    args[0].asString(jsRuntime).utf8(jsRuntime)),
                url);
          }));
}

void QtReactHost::installTurboModuleBinding() {
  // RN 0.88 deprecates this invoker in favor of ReactInstance's buffered
  // executor. This standalone synchronous host does not own a ReactInstance
  // or its callable-module queue. Keep RN's compatibility path, just as
  // ReactInstance::createJSCallInvoker() does with buffering disabled. Scope
  // the diagnostic to this exact use; migrating the host to a buffered JS
  // queue is a separate runtime change, not a reason to disable -Werror.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  auto callInvoker =
      std::make_shared<facebook::react::RuntimeSchedulerCallInvoker>(
          impl_->runtimeScheduler);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  auto* impl = impl_.get();

  // View events must be forwarded even when JavaScript never looks up
  // ImageLoader. One host-owned subscription also avoids duplicate forwarding
  // when the binding is reinstalled or the loader is created later.
  views::ImageLoadedNotifier::shared().unsubscribe(impl->imageLoadedSubscription);
  impl->imageLoadedSubscription = views::ImageLoadedNotifier::shared().subscribe(
      [callInvoker](const views::ImageLoadedEvent& event) {
        callInvoker->invokeAsync([event](facebook::jsi::Runtime& rt) { emitImageLoaded(rt, event); });
      });

  auto createModule = [callInvoker, impl](
          facebook::jsi::Runtime& /*jsRuntime*/,
          const std::string& name)
          -> std::shared_ptr<facebook::react::TurboModule> {
        if (impl->turboModuleProvider) {
          if (auto module = impl->turboModuleProvider(name, callInvoker)) {
            return module;
          }
        }

        // DevSettings.js imports NativeDevSettings, which calls
        // getEnforcing('DevSettings') at module load in release bundles too
        // (React Native's own RCTDevSettings exists in iOS release builds).
        // Without a development client the module accepts every call and
        // does nothing, which is what DevSettings promises in release.
        if (name == "DevSettings") {
          return makeDevSupportModule(name, callInvoker, {});
        }

        // Without this, every JavaScript error aborts the process: React
        // reaches for ExceptionsManager while reporting, getEnforcing throws,
        // and the throw escapes.
        if (name == QnxDeviceInfo::kModuleName) {
          // Dimensions reads this, and ScrollView reads Dimensions.
          return std::make_shared<QnxDeviceInfo>(callInvoker);
        }

        if (name == QnxSourceCode::kModuleName) {
          // getDevServer and resolveAssetSource read scriptURL from this, so
          // <Image> cannot render without it. The value is the bundle the host
          // actually evaluated.
          return std::make_shared<QnxSourceCode>(callInvoker, impl->scriptUrl);
        }

        if (name == QnxExceptionsManager::kModuleName) {
          return std::make_shared<QnxExceptionsManager>(
              callInvoker, [impl](const JavaScriptError& error) {
                qWarning().noquote()
                    << (error.isFatal ? "[js fatal]" : "[js error]")
                    << QString::fromStdString(error.message) << '\n'
                    << QString::fromStdString(error.stack);
                impl->javaScriptErrors.push_back(error);
              });
        }

        if (name == facebook::react::AnimatedModule::kModuleName) {
          auto startOnRender =
              [impl](std::function<void()>&& onRender, bool /*isAsync*/) {
                impl->animationFrame = std::move(onRender);
                if (!impl->animationTimer) {
                  impl->animationTimer = std::make_unique<QTimer>();
                  impl->animationTimer->setInterval(16);
                  QObject::connect(
                      impl->animationTimer.get(),
                      &QTimer::timeout,
                      [impl]() {
                        if (impl->animationFrame) {
                          impl->animationFrame();
                        }
                      });
                }
                impl->animationTimer->start();
              };
          auto stopOnRender =
              [impl](bool /*isAsync*/) {
                impl->animationFrame = nullptr;
                if (impl->animationTimer) {
                  impl->animationTimer->stop();
                }
              };
          return std::make_shared<facebook::react::AnimatedModule>(
              callInvoker,
              std::make_shared<facebook::react::NativeAnimatedNodesManagerProvider>(
                  std::move(startOnRender), std::move(stopOnRender)));
        }

        if (name == rnqnx::runtime::QnxImageLoader::kModuleName) {
          return std::make_shared<rnqnx::runtime::QnxImageLoader>(callInvoker);
        }

        // No-op modules for platform APIs that are referenced by React Native
        // bundles but not wired to a QNX-native backend. Installing them
        // prevents getEnforcing from throwing during TurboModule resolution;
        // each method reports its unsupported status honestly.
        if (name == rnqnx::runtime::QnxWebSocketModule::kModuleName) {
          return std::make_shared<rnqnx::runtime::QnxWebSocketModule>(
              callInvoker);
        }
        if (name == rnqnx::runtime::QnxFileReaderModule::kModuleName) {
          return std::make_shared<rnqnx::runtime::QnxFileReaderModule>(
              callInvoker);
        }
        if (name == rnqnx::runtime::QnxSegmentFetcher::kModuleName) {
          return std::make_shared<rnqnx::runtime::QnxSegmentFetcher>(callInvoker);
        }

#if RNQNX_ENABLE_APP_STATE
        if (name == QnxAppStateModule::kModuleName) {
          if (!impl->appStateModule) {
            impl->appStateModule = std::make_shared<QnxAppStateModule>(callInvoker);
          }
          return impl->appStateModule;
        }
#endif

        // React Native's own generic C++ modules. Two of the modules this
        // platform needs come from here already implemented — microtasks and
        // idle callbacks, both backed by the RuntimeScheduler.
        return facebook::react::DefaultTurboModules::getTurboModule(
            name, callInvoker);
      };
  facebook::react::TurboModuleBinding::install(
      *impl_->runtime,
      [impl, createModule = std::move(createModule)](
          facebook::jsi::Runtime& runtime, const std::string& name) {
        if (const auto found = impl->turboModules.find(name); found != impl->turboModules.end())
          return found->second;
        auto module = createModule(runtime, name);
        if (module) impl->turboModules.emplace(name, module);
        return module;
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
  version.setProperty(runtime, "minor", 88);
  version.setProperty(runtime, "patch", 0);
  version.setProperty(
      runtime,
      "prerelease",
      facebook::jsi::String::createFromUtf8(runtime, "rc.3"));
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
                // Timer callbacks are JS tasks too. A direct JSI call skips
                // RN's microtask/rendering checkpoint and can strand updates.
                impl->runtimeScheduler->scheduleWork(
                    [callback](facebook::jsi::Runtime& runtime) {
                      callback->call(runtime);
                    });
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
  // A surface's viewport is fixed by its host window. A zero minimum allows
  // Yoga to shrink-wrap the root, leaving flex and percentage children with
  // an intrinsic-width viewport instead of the supplied surface dimensions.
  constraints.minimumSize = {.width = width, .height = height};
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
#if RNQNX_ENABLE_FRAME_TRACE
  const auto generation = impl_->trace ? impl_->trace->nextId() : 0;
  TraceSpan trace(impl_->trace, TraceStage::SurfaceStart, surfaceId, generation);
#endif
  auto handler = std::make_unique<SurfaceHandler>(moduleName, surfaceId);
  impl_->scheduler->registerSurface(*handler);
  handler->setProps(folly::dynamic::object());
  handler->constraintLayout(
      makeLayoutConstraints(width, height), facebook::react::LayoutContext{});
  handler->start();
  impl_->surfaces.emplace(surfaceId, std::move(handler));
#if RNQNX_ENABLE_FRAME_TRACE
  if (impl_->trace) impl_->surfaceGenerations[surfaceId] = generation;
#endif
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

#if RNQNX_ENABLE_FRAME_TRACE
  const auto generation = impl_->trace ? impl_->trace->nextId() : 0;
  TraceSpan trace(impl_->trace, TraceStage::SurfaceStart, surfaceId, generation);
#endif
  auto handler = std::make_unique<SurfaceHandler>("", surfaceId);
  impl_->scheduler->registerSurface(*handler);

  handler->constraintLayout(
      makeLayoutConstraints(width, height), facebook::react::LayoutContext{});
  handler->start();

  impl_->surfaces.emplace(surfaceId, std::move(handler));
#if RNQNX_ENABLE_FRAME_TRACE
  if (impl_->trace) impl_->surfaceGenerations[surfaceId] = generation;
#endif
}

void QtReactHost::resizeSurface(SurfaceId surfaceId, float width, float height) {
  const auto found = impl_->surfaces.find(surfaceId);
  if (found == impl_->surfaces.end() || width <= 0 || height <= 0) {
    return;
  }
  found->second->constraintLayout(
      makeLayoutConstraints(width, height), facebook::react::LayoutContext{});
}

void QtReactHost::setWindowSize(float width, float height, float scale) {
  if (width <= 0 || height <= 0) return;
  if (impl_->windowWidth == width && impl_->windowHeight == height && impl_->windowScale == scale) {
    return;
  }
  impl_->windowWidth = width;
  impl_->windowHeight = height;
  impl_->windowScale = scale;
  // The shape Dimensions.set reads: { window, screen }, each with width,
  // height, scale and fontScale. The screen keeps the device's size.
  const auto metrics = [scale](double w, double h) {
    return folly::dynamic::object("width", w)("height", h)("scale", scale)("fontScale", 1.0);
  };
  double screenWidth = width;
  double screenHeight = height;
  if (const auto* screen = QGuiApplication::primaryScreen()) {
    screenWidth = screen->geometry().width();
    screenHeight = screen->geometry().height();
  }
  callFunctionOnModule(
      "RCTDeviceEventEmitter", "emit",
      folly::dynamic::array(
          "didUpdateDimensions",
          folly::dynamic::object("window", metrics(width, height))(
              "screen", metrics(screenWidth, screenHeight))),
      /*ifRegistered=*/true);
}

void QtReactHost::stopSurface(SurfaceId surfaceId) {
#if RNQNX_ENABLE_FRAME_TRACE
  const auto generation = impl_->surfaceGenerations.find(surfaceId);
  TraceSpan trace(impl_->trace, TraceStage::SurfaceStop, surfaceId,
                  generation == impl_->surfaceGenerations.end() ? 0 : generation->second);
#endif
  const auto found = impl_->surfaces.find(surfaceId);
  if (found == impl_->surfaces.end()) {
    return;
  }

  if (found->second->getStatus() == SurfaceHandler::Status::Running) {
    found->second->stop();
  }
  impl_->scheduler->unregisterSurface(*found->second);
  impl_->surfaces.erase(found);
#if RNQNX_ENABLE_FRAME_TRACE
  impl_->surfaceGenerations.erase(surfaceId);
#endif
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
