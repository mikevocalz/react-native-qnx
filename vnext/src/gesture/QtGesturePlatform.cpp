#include "react-native-qnx/gesture/QtGesturePlatform.h"

#include "QnxGestureHandlerModule.h"
#include "QtGestureInput.h"
#include "react-native-qnx/gesture/GestureHandlerBindings.h"
#include "react-native-qnx/fabric/ViewObserver.h"

#include <react/renderer/components/rngesturehandler_codegen/ComponentDescriptors.h>
#include <react/renderer/components/rngesturehandler_codegen/EventEmitters.h>
#include <react/renderer/components/rngesturehandler_codegen/Props.h>
#include <react/renderer/componentregistry/ComponentDescriptorProvider.h>
#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include <runtime/RNGHRuntimeDecorator.h>
#include <worklets/WorkletRuntime/WorkletRuntime.h>

#include <QQuickItem>
#include <QQuickWindow>

#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace rnqnx::gesture {

namespace {

using facebook::react::RNGestureHandlerDetectorEventEmitter;
using facebook::react::RNGestureHandlerDetectorProps;

constexpr int kReanimatedWorklet = 1;

bool traceEnabled() {
  static const bool enabled = std::getenv("RNQNX_GESTURE_TRACE") != nullptr;
  return enabled;
}

folly::dynamic handlerDataFor(const GestureEvent& e) {
  folly::dynamic data = folly::dynamic::object;
  data["x"] = e.data.x;
  data["y"] = e.data.y;
  data["absoluteX"] = e.data.absoluteX;
  data["absoluteY"] = e.data.absoluteY;
  data["pointerInside"] = e.data.pointerInside;
  data["numberOfPointers"] = e.data.numberOfPointers;
  data["pointerType"] = 0;
  if (e.handlerName == "PanGestureHandler") {
    data["translationX"] = e.data.translationX;
    data["translationY"] = e.data.translationY;
    data["velocityX"] = e.data.velocityX;
    data["velocityY"] = e.data.velocityY;
  } else if (e.handlerName == "LongPressGestureHandler") {
    data["duration"] = e.data.duration;
  } else if (e.handlerName == "FlingGestureHandler") {
    data["direction"] = e.data.direction;
  }
  return data;
}

class Platform final : public QtGesturePlatform, public std::enable_shared_from_this<Platform> {
 public:
  Platform(runtime::QtReactHost& host, fabric::ShadowViewMutationAdapter& adapter,
           fabric::QtQuickMountingLayer& layer, QQuickWindow& window,
           std::shared_ptr<worklets::QtWorkletsPlatform> worklets)
      : host_(host), adapter_(adapter), layer_(layer), worklets_(std::move(worklets)),
        input_(new QtGestureInput(registry_, window)) {}

  Registry& registry() override { return registry_; }

  void init() {
    registry_.setHitTest([this](int viewTag, double x, double y) { return hit(viewTag, x, y); });
    registry_.setSink([this](const GestureEvent& e) { dispatch(e); });
    fabric::setViewObserver([this](const facebook::react::ShadowView& view, bool removed) { observe(view, removed); });
  }

  void shutdown() {
    fabric::setViewObserver(nullptr);
    registry_.setSink(nullptr);
    emitters_.clear();
    delete input_;  // QObject child of the window; safe to delete early
    input_ = nullptr;
  }

  std::shared_ptr<facebook::react::TurboModule> makeModule() {
    QnxGestureHandlerModule::Hooks hooks;
    auto weak = weak_from_this();
    hooks.create = [weak](const std::string& name, int tag, const Config& config) {
      if (auto self = weak.lock()) {
        self->bindings_.create(name, tag, config);
        if (!self->registry_.unsupported().empty() &&
            self->registry_.unsupported().back() == name) {
          std::fprintf(stderr, "[gesture-handler] %s is not supported on QNX; it will fail on first touch\n", name.c_str());
        }
      }
    };
    hooks.attach = [weak](int tag, int viewTag, int actionType) {
      if (auto self = weak.lock()) self->registry_.attach(tag, viewTag, actionType);
    };
    hooks.setConfig = [weak](int tag, const Config& config) {
      if (auto self = weak.lock()) self->bindings_.configure(tag, config);
    };
    hooks.relations = [weak](int tag, const Relations& r) {
      if (auto self = weak.lock()) self->registry_.setRelations(tag, r);
    };
    hooks.drop = [weak](int tag) {
      if (auto self = weak.lock()) {
        self->bindings_.drop(tag);
      }
    };
    hooks.installBindings = [weak](facebook::jsi::Runtime& rt) {
      auto self = weak.lock();
      if (!self) return false;
      auto setState = [weak](int tag, int state) {
        if (auto s = weak.lock()) {
          s->worklets_->postToGui([weak, tag, state]() {
            if (auto g = weak.lock()) g->registry_.setState(tag, static_cast<State>(state));
          });
        }
      };
      gesturehandler::RNGHRuntimeDecorator::installRNRuntimeBindings(rt, std::function<void(int, int)>(setState));
      const auto resolved = gesturehandler::RNGHRuntimeDecorator::tryFindUIRuntime(rt);
      if (resolved.runtime != nullptr) {
        gesturehandler::RNGHRuntimeDecorator::installUIRuntimeBindings(
            *resolved.runtime, std::function<void(int, int)>(setState));
      }
      return resolved.runtime != nullptr;
    };
    return std::make_shared<QnxGestureHandlerModule>(worklets_->jsCallInvoker(), std::move(hooks));
  }

 private:
  ViewHit hit(int viewTag, double x, double y) {
    ViewHit out;
    QQuickItem* item = layer_.itemForTag(viewTag);
    if (item == nullptr) {
      if (traceEnabled()) std::fprintf(stderr, "RNQNX_GESTURE_HIT tag=%d missing=1\n", viewTag);
      return out;
    }
    // The detector is `display: contents`: its own box can be empty, so the
    // attached area is the union with its children.
    QRectF area = QRectF(0, 0, item->width(), item->height()).united(item->childrenRect());
    const QPointF local = item->mapFromScene(QPointF(x, y));
    out.contains = area.contains(local);
    out.localX = local.x();
    out.localY = local.y();
    if (traceEnabled()) std::fprintf(stderr,
        "RNQNX_GESTURE_HIT tag=%d scene=%.1f,%.1f local=%.1f,%.1f area=%.1f,%.1f,%.1f,%.1f contains=%d\n",
        viewTag, x, y, local.x(), local.y(), area.x(), area.y(), area.width(), area.height(), out.contains);
    return out;
  }

  void observe(const facebook::react::ShadowView& view, bool removed) {
    if (view.componentName == nullptr || std::string_view(view.componentName) != "RNGestureHandlerDetector") return;
    if (removed) {
      emitters_.erase(view.tag);
      bindings_.removeView(view.tag);
      return;
    }
    if (auto emitter = std::dynamic_pointer_cast<const RNGestureHandlerDetectorEventEmitter>(view.eventEmitter)) {
      emitters_[view.tag] = emitter;
    }
    const auto props = std::dynamic_pointer_cast<const RNGestureHandlerDetectorProps>(view.props);
    if (!props) return;
    std::vector<int> tags(props->handlerTags.begin(), props->handlerTags.end());
    bindings_.observe(view.tag, tags);
  }

  void dispatch(const GestureEvent& e) {
    if (traceEnabled()) std::fprintf(stderr,
        "RNQNX_GESTURE_EVENT handler=%d view=%d route=%d state=%d old=%d kind=%d\n",
        e.handlerTag, e.viewTag, e.actionType, static_cast<int>(e.state),
        static_cast<int>(e.oldState), static_cast<int>(e.kind));
    const auto it = emitters_.find(e.viewTag);
    if (it == emitters_.end()) return;
    const auto& emitter = *it->second;
    const int state = static_cast<int>(e.state);
    const int old = static_cast<int>(e.oldState);
    auto data = handlerDataFor(e);
    if (e.actionType == kReanimatedWorklet) {
      if (e.kind == EventKind::StateChange) {
        emitter.onGestureHandlerReanimatedStateChange({e.handlerTag, state, old, std::move(data)});
      } else {
        emitter.onGestureHandlerReanimatedEvent({e.handlerTag, state, std::move(data)});
      }
    } else if (e.kind == EventKind::StateChange) {
      emitter.onGestureHandlerStateChange({e.handlerTag, state, old, std::move(data)});
    } else {
      emitter.onGestureHandlerEvent({e.handlerTag, state, std::move(data)});
    }
  }

  runtime::QtReactHost& host_;
  fabric::ShadowViewMutationAdapter& adapter_;
  fabric::QtQuickMountingLayer& layer_;
  std::shared_ptr<worklets::QtWorkletsPlatform> worklets_;
  Registry registry_;
  QtGestureInput* input_;
  DetectorBindings bindings_{registry_};
  std::unordered_map<int, std::shared_ptr<const RNGestureHandlerDetectorEventEmitter>> emitters_;
};

}  // namespace

void registerComponents() {
  using namespace facebook::react;
  runtime::QtReactHost::addComponentDescriptorProvider(
      "RNGestureHandlerDetector", concreteComponentDescriptorProvider<RNGestureHandlerDetectorComponentDescriptor>());
  runtime::QtReactHost::addComponentDescriptorProvider(
      "RNGestureHandlerRootView", concreteComponentDescriptorProvider<RNGestureHandlerRootViewComponentDescriptor>());
  runtime::QtReactHost::addComponentDescriptorProvider(
      "RNGestureHandlerButton", concreteComponentDescriptorProvider<RNGestureHandlerButtonComponentDescriptor>());
}

std::shared_ptr<QtGesturePlatform> QtGesturePlatform::install(
    runtime::QtReactHost& host, fabric::ShadowViewMutationAdapter& adapter,
    fabric::QtQuickMountingLayer& layer, QQuickWindow& window,
    const std::shared_ptr<worklets::QtWorkletsPlatform>& worklets) {
  auto platform = std::make_shared<Platform>(host, adapter, layer, window, worklets);
  platform->init();
  // Mounted as plain containers; the detector forwards its children.
  adapter.mapComponent(QStringLiteral("RNGestureHandlerDetector"), QStringLiteral("View"));
  adapter.mapComponent(QStringLiteral("RNGestureHandlerRootView"), QStringLiteral("View"));
  adapter.mapComponent(QStringLiteral("RNGestureHandlerButton"), QStringLiteral("View"));
  host.registerTurboModule(QnxGestureHandlerModule::kModuleName,
                           [platform](std::shared_ptr<facebook::react::CallInvoker>) { return platform->makeModule(); });
  host.addTeardownHook([platform]() { platform->shutdown(); });
  return platform;
}

}  // namespace rnqnx::gesture
