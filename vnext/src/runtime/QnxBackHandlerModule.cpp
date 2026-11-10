#include "react-native-qnx/runtime/QnxBackHandlerModule.h"

#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QThread>
#include <QWindow>
#include <cstdint>
#include <stdexcept>

namespace rnqnx::runtime {
namespace jsi = facebook::jsi;
namespace react = facebook::react;

struct QnxBackHandlerModule::Impl final : QObject, std::enable_shared_from_this<Impl> {
  explicit Impl(std::shared_ptr<react::CallInvoker> invoker)
      : invoker(std::move(invoker)) {}

  void requireOwner() const noexcept {
    if (QThread::currentThread() != thread()) std::terminate();
  }

  void setWindow(QWindow* next) {
    requireOwner();
    if (!active) throw std::logic_error("BackHandler observation is closed");
    if (next && next->thread() != thread()) {
      throw std::logic_error("BackHandler window must belong to the runtime owner");
    }
    if (window) window->removeEventFilter(this);
    ++generation;
    window = next;
    if (window) window->installEventFilter(this);
  }

  bool eventFilter(QObject* watched, QEvent* event) override {
    requireOwner();
    if (!active || watched != window ||
        (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)) return false;
    const auto* key = static_cast<QKeyEvent*>(event);
    // Escape is intentionally not a system Back mapping: editors and modals
    // may own it. Only an actual Qt Back key without modifiers is observed.
    if (key->key() != Qt::Key_Back || key->modifiers() != Qt::NoModifier) return false;
    if (event->type() == QEvent::KeyPress || key->isAutoRepeat()) return true;
    const auto acceptedGeneration = generation;
    const std::weak_ptr<Impl> weak = shared_from_this();
    invoker->invokeAsync([weak, acceptedGeneration](jsi::Runtime& runtime) {
      const auto state = weak.lock();
      if (!state || !state->active || state->generation != acceptedGeneration || !state->window) return;
      state->requireOwner();
      // One existing runtime queue hop, no saved Runtime&, JSI values, or timers.
      // RN invokes the fallback synchronously while emitting this event.
      state->dispatchWindow = state->window;
      state->dispatching = true;
      state->defaultInvoked = false;
      try {
        auto emitter = runtime.global().getProperty(runtime, "__rctDeviceEventEmitter");
        if (emitter.isObject()) {
          auto object = emitter.asObject(runtime);
          auto listenerCount = object.getProperty(runtime, "listenerCount");
          const bool canCount = listenerCount.isObject() && listenerCount.asObject(runtime).isFunction(runtime);
          const auto count = canCount
              ? listenerCount.asObject(runtime).asFunction(runtime).callWithThis(runtime, object,
                  jsi::String::createFromAscii(runtime, "hardwareBackPress"))
              : jsi::Value(0.0);
          if (!count.isNumber() || count.getNumber() <= 0) {
            // InitializeCore can install the emitter before BackHandler is
            // imported. An emitter without its RN listener is unhandled Back.
            state->closeWindow();
          } else {
            object.getPropertyAsFunction(runtime, "emit").callWithThis(runtime, object,
                jsi::String::createFromAscii(runtime, "hardwareBackPress"));
          }
        } else {
          state->closeWindow();
        }
      } catch (...) {
        state->dispatching = false;
        state->dispatchWindow.clear();
        throw;
      }
      state->dispatching = false;
      state->dispatchWindow.clear();
    });
    return true;
  }

  void closeWindow() {
    requireOwner();
    if (dispatching && defaultInvoked) return;
    auto target = dispatching ? dispatchWindow : window;
    if (!target) return;
    if (dispatching) defaultInvoked = true;
    // Close only the attached window, never quit unrelated application hosts.
    target->close();
  }

  std::shared_ptr<react::CallInvoker> invoker;
  QPointer<QWindow> window;
  QPointer<QWindow> dispatchWindow;
  std::uint64_t generation{0};
  bool active{true};
  bool dispatching{false};
  bool defaultInvoked{false};
};

QnxBackHandlerModule::QnxBackHandlerModule(std::shared_ptr<react::CallInvoker> invoker)
    : react::NativeDeviceEventManagerCxxSpec<QnxBackHandlerModule>(invoker) {
  auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (!app || QThread::currentThread() != app->thread() || !invoker) {
    throw std::logic_error("BackHandler requires QGuiApplication owner and runtime invoker");
  }
  impl_ = std::make_shared<Impl>(std::move(invoker));
}

QnxBackHandlerModule::~QnxBackHandlerModule() { invalidate(); }

void QnxBackHandlerModule::setWindow(QWindow* window) {
  if (!impl_) throw std::logic_error("BackHandler observation is closed");
  impl_->setWindow(window);
}

void QnxBackHandlerModule::invalidate() noexcept {
  if (!impl_ || !impl_->active) return;
  impl_->requireOwner();
  impl_->setWindow(nullptr);
  impl_->active = false;
  impl_->invoker.reset();
  impl_.reset();
}

void QnxBackHandlerModule::invokeDefaultBackPressHandler(jsi::Runtime& runtime) {
  auto state = impl_;
  if (!state) throw jsi::JSError(runtime, "BackHandler observation is closed");
  state->requireOwner();
  if (!state->active || !state->window) {
    throw jsi::JSError(runtime, "BackHandler requires an attached live host window");
  }
  state->closeWindow();
}

}  // namespace rnqnx::runtime
