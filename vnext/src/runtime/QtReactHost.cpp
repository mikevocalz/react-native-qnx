#include "react-native-qnx/runtime/QtReactHost.h"

#include <hermes/hermes.h>

#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/FBReactNativeSpec/ComponentDescriptors.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>
#include <react/renderer/core/ShadowNodeFragment.h>
#include <react/renderer/mounting/MountingCoordinator.h>
#include <react/renderer/mounting/MountingTransaction.h>
#include <react/renderer/mounting/ShadowTree.h>
#include <react/renderer/runtimescheduler/RuntimeSchedulerBinding.h>
#include <react/renderer/scheduler/SchedulerToolbox.h>
#include <react/renderer/uimanager/UIManager.h>

#include <QCoreApplication>
#include <QMetaObject>

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
    runtime = facebook::hermes::makeHermesRuntime();

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

bool QtReactHost::hasFabricUIManager() {
  auto& runtime = *impl_->runtime;
  const auto value = runtime.global().getProperty(runtime, "nativeFabricUIManager");
  return !value.isUndefined() && !value.isNull();
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

  facebook::react::LayoutConstraints constraints;
  constraints.minimumSize = {.width = 0, .height = 0};
  constraints.maximumSize = {.width = width, .height = height};
  // SurfaceHandler.cpp:43 asserts the direction is set, and it defaults to
  // Undefined, so this must happen before start().
  constraints.layoutDirection = facebook::react::LayoutDirection::LeftToRight;

  handler->constraintLayout(constraints, facebook::react::LayoutContext{});
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
