// Link and run the actual Worklets/Reanimated/Gesture Handler packages on the
// Qt/Hermes/Fabric host. This is Linux host evidence, not QNX device evidence.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/fabric/ShadowViewMutationAdapter.h"
#include "react-native-qnx/gesture/QtGesturePlatform.h"
#include "react-native-qnx/reanimated/QtReanimatedPlatform.h"
#include "react-native-qnx/runtime/QtReactHost.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include "react-native-qnx/worklets/QtWorkletsPlatform.h"

#include <react/renderer/core/EventListener.h>

#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#include <atomic>
#include <cmath>
#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr facebook::react::SurfaceId kSurfaceId = 11;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

std::string readBundle() {
  std::ifstream input(RNQNX_WORKLETS_INTEGRATION_BUNDLE_PATH);
  require(input.good(), "integration Metro bundle is missing");
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

class Fixture {
 public:
  explicit Fixture(bool installExtensions)
      : factory_(engine_), layer_(factory_, *window_.contentItem(), kSurfaceId),
        adapter_(layer_), host_(adapter_) {
    factory_.registerType(QStringLiteral("View"), QByteArray(
        "import QtQuick\n"
        "Rectangle { id: root; visible: true; property alias backgroundColor: root.color }\n"));
    adapter_.mapComponent(QStringLiteral("View"), QStringLiteral("View"));
    window_.resize(240, 180);
    window_.setColor(Qt::white);
    if (installExtensions) {
      worklets_ = rnqnx::worklets::QtWorkletsPlatform::install(host_, &window_);
      rnqnx::reanimated::installReanimated(host_, worklets_);
      gestures_ = rnqnx::gesture::QtGesturePlatform::install(
          host_, adapter_, layer_, window_, worklets_);
    }
    host_.installTurboModuleBinding();
    host_.installComponentProvider();
    host_.installPlatformConstants();
    host_.installTimers();
#if RNQNX_ENABLE_QT_RNSVG
    host_.runtime().global().setProperty(host_.runtime(), "__rnqnxSvgInterestEnabled", true);
    host_.scheduler().addEventListener(std::make_shared<const facebook::react::EventListener>(
        [this](const facebook::react::RawEvent& event) {
          if (event.type == "topSvgLayout") ++svgDispatches_;
          return false;
        }));
#endif
  }

  ~Fixture() { host_.stopSurface(kSurfaceId); }

  bool load(const std::string& bundle) {
    const bool evaluated = host_.evaluate(bundle, "worklets-integration.bundle.js");
    return evaluated && error().empty();
  }

  bool missingBootstrapFailure(bool evaluated) {
    if (evaluated && error().empty()) return false;
    auto& runtime = host_.runtime();
    if (!runtime.global().getProperty(runtime, "__workletsModuleProxy").isUndefined()) return false;
    const auto failure = error();
    const auto message = failure.substr(0, failure.find('\n'));
    // Match the pinned package's actual failure, never its stack/source URL.
    // 0.13.1 may dereference the missing proxy in installUnpackers before its
    // explicit initialization diagnostic. Both paths require an absent proxy.
    return message.find("[Worklets] Native part of Worklets doesn't seem to be initialized.") != std::string::npos ||
        message.find("Cannot read property 'loadUnpackersWithCode' of undefined") != std::string::npos ||
        message.find("Cannot read property 'loadUnpackersWithBytecode' of undefined") != std::string::npos;
  }

  void start() {
    window_.show();
    host_.startSurface(kSurfaceId, "RnqnxWorkletsIntegration", 240, 180);
    host_.setWindowSize(240, 180, static_cast<float>(window_.devicePixelRatio()));
  }

  void waitFor(const std::string& operation, const std::function<bool()>& ready) {
    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < 5000) {
      host_.runtime().drainMicrotasks();
      window_.update();
      host_.pumpEvents();
      require(error().empty(), operation + ": " + error());
      require(host_.delegate().lastFailures().empty(), operation + ": Fabric mount failed");
      if (ready()) return;
    }
    throw std::runtime_error(operation + " did not complete before the 5-second deadline");
  }

  facebook::jsi::Value value(const char* name) {
    auto& runtime = host_.runtime();
    const auto proof = runtime.global().getPropertyAsObject(runtime, "__rnqnxIntegration");
    return proof.getProperty(runtime, name);
  }

  bool flag(const char* name) {
    auto property = value(name);
    return property.isBool() && property.getBool();
  }

  double number(const char* name) { return value(name).asNumber(); }

  void evaluate(const char* source) {
    const bool evaluated = host_.evaluate(source, "integration-driver.js");
    require(evaluated && error().empty(), error());
  }

  bool origin(const char* tag, double x, double y) {
    const auto* item = layer_.itemForTag(static_cast<int>(number(tag)));
    if (!item) return false;
    const QPointF point = item->mapToScene(QPointF(0, 0));
    return std::abs(point.x() - x) < 0.1 && std::abs(point.y() - y) < 0.1;
  }

  bool pixel(int x, int y, const QColor& color) {
    const QImage image = window_.grabWindow();
    if (image.isNull()) return false;
    const double ratio = window_.devicePixelRatio();
    return image.pixelColor(static_cast<int>(x * ratio), static_cast<int>(y * ratio)) == color;
  }

  void saveFrame(int trial, const char* phase) {
    const QString directory = qEnvironmentVariable("RNQNX_WORKLETS_EVIDENCE_DIR");
    if (directory.isEmpty()) return;
    require(QDir().mkpath(directory), "could not create integration evidence directory");
    const QImage image = window_.grabWindow();
    const QString name = QStringLiteral("%1-%2.png").arg(trial).arg(QString::fromUtf8(phase));
    require(!image.isNull() && image.save(QDir(directory).filePath(name)),
            "could not save integration frame " + name.toStdString());
  }

  void mouse(QEvent::Type type, int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons) {
    const QPointF point(x, y);
    QMouseEvent event(type, point, point, point, button, buttons, Qt::NoModifier,
                      Qt::MouseEventNotSynthesized);
    QCoreApplication::sendEvent(&window_, &event);
  }

  void exercise(int trial) {
    waitFor("React mount and effects", [&] { return flag("ready"); });
    require(layer_.itemForTag(static_cast<int>(number("timingTag"))) != nullptr,
            "Animated.View was not mounted by Fabric");
    require(layer_.itemForTag(static_cast<int>(number("gestureTag"))) != nullptr,
            "GestureDetector child was not mounted by Fabric");
    waitFor("initial pixels", [&] {
      return origin("timingTag", 20, 20) && origin("gestureTag", 20, 100) &&
          pixel(30, 30, QColor(Qt::red)) && pixel(30, 110, QColor(Qt::blue));
    });
    saveFrame(trial, "initial");
    waitFor("Worklets frame round trip", [&] { return value("frames").isObject(); });
    auto& runtime = host_.runtime();
    auto frames = value("frames").asObject(runtime);
    require(frames.getProperty(runtime, "answer").asNumber() == 42, "UI/RN argument round trip failed");
    require(frames.getProperty(runtime, "count").asNumber() == 3, "Qt frame chain was incomplete");
    require(frames.getProperty(runtime, "monotonic").getBool(), "frame timestamps went backwards");
    require(frames.getProperty(runtime, "role").asString(runtime).utf8(runtime) == "ui-worklet",
            "worklet ran on the wrong thread");

    evaluate("globalThis.__rnqnxStartTiming();");
    waitFor("Reanimated timing callback", [&] { return flag("timingDone"); });
    waitFor("Reanimated rendered transform", [&] {
      return origin("timingTag", 80, 20) &&
          pixel(30, 30, QColor(Qt::white)) && pixel(90, 30, QColor(Qt::red));
    });
    saveFrame(trial, "timing");

    mouse(QEvent::MouseButtonPress, 30, 110, Qt::LeftButton, Qt::LeftButton);
    mouse(QEvent::MouseMove, 45, 110, Qt::NoButton, Qt::LeftButton);
    mouse(QEvent::MouseMove, 70, 110, Qt::NoButton, Qt::LeftButton);
    waitFor("Gesture Handler worklet update", [&] {
      auto update = value("panUpdate");
      return update.isNumber() && std::abs(update.asNumber() - 40) < 0.1;
    });
    waitFor("gesture rendered transform", [&] {
      return origin("gestureTag", 60, 100) &&
          pixel(30, 110, QColor(Qt::white)) && pixel(70, 110, QColor(Qt::blue));
    });
    saveFrame(trial, "pan");
    mouse(QEvent::MouseButtonRelease, 70, 110, Qt::LeftButton, Qt::NoButton);
    waitFor("Gesture Handler terminal callback", [&] {
      auto end = value("panEnd");
      return end.isNumber() && std::abs(end.asNumber() - 40) < 0.1;
    });

#if RNQNX_ENABLE_QT_RNSVG
    exerciseSvgInterest();
#endif

    host_.stopSurface(kSurfaceId);
    waitFor("React unmount", [&] { return flag("unmounted"); });
    require(window_.contentItem()->childItems().empty(), "surface teardown left mounted views");
  }

  void queueBeforeDestruction() {
    waitFor("initial frame probe before teardown", [&] { return value("frames").isObject(); });
#if RNQNX_ENABLE_QT_RNSVG
    // Leave a handler that has delivered real events installed for teardown.
    exerciseSvgInterest();
#endif
    evaluate("globalThis.__rnqnxQueueBeforeTeardown();");
    // FIFO UI barrier after scheduleOnUI's microtask. Do not pump the GUI: the
    // admitted worklet must request a frame that cannot be delivered before
    // destruction. A no-op scheduleOnUI therefore fails this trial.
    waitForUiQueue("pending frame before teardown");
    // Frame requests may coalesce with a request already in the library's
    // batch. Read the shared value through Reanimated's public RN getter:
    // 99 proves the worklet ran and enqueued its callback; 100 would mean the
    // callback already ran. Neither a no-op scheduler nor completed work passes.
    evaluate("globalThis.__rnqnxAdmissionProof = globalThis.__rnqnxTeardownAdmission();");
    require(host_.runtime().global().getProperty(host_.runtime(), "__rnqnxAdmissionProof").asNumber() == 99,
            "teardown trial did not retain an admitted worklet frame callback");
  }

  std::string error() const {
    if (!host_.lastError().isEmpty()) return host_.lastError().toStdString();
    // Metro handles module throws through ErrorUtils/ExceptionsManager. They
    // can be reported without a direct JSI evaluate exception or lastError.
    const auto& reported = host_.javaScriptErrors();
    return reported.empty() ? std::string{} : reported.front().message;
  }

 private:
  void waitForUiQueue(const std::string& operation) {
    host_.runtime().drainMicrotasks();
    const auto barrier = std::make_shared<std::promise<void>>();
    auto completed = barrier->get_future();
    require(worklets_->postToUi([barrier] { barrier->set_value(); }), operation + ": UI barrier admission failed");
    require(completed.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
            operation + ": UI barrier did not complete");
    completed.get();
  }

#if RNQNX_ENABLE_QT_RNSVG
  void flushWorkletQueues(const std::string& operation) {
    // Registry mutations and raw event delivery use the UI FIFO. Only post the
    // RN barrier after that FIFO finishes, so callbacks it scheduled precede it.
    waitForUiQueue(operation);
    const auto completed = std::make_shared<std::atomic<bool>>(false);
    worklets_->jsCallInvoker()->invokeAsync([completed] { completed->store(true); });
    waitFor(operation, [&] { return completed->load(); });
  }

  size_t svgCallbackCount() {
    auto& runtime = host_.runtime();
    return value("svgLayouts").asObject(runtime).asArray(runtime).size(runtime);
  }

  void requireSvgCallbacks(size_t count, double width) {
    auto& runtime = host_.runtime();
    auto callbacks = value("svgLayouts").asObject(runtime).asArray(runtime);
    require(callbacks.size(runtime) == count, "unexpected SVG worklet callback count");
    require(svgDispatches_.load() == static_cast<int>(count), "unexpected native SVG dispatch count");
    if (count == 0) return;
    auto report = callbacks.getValueAtIndex(runtime, count - 1).asObject(runtime);
    require(report.getProperty(runtime, "role").asString(runtime).utf8(runtime) == "ui-worklet",
            "SVG event handler ran on the wrong thread");
    auto layout = report.getPropertyAsObject(runtime, "layout");
    require(layout.getProperty(runtime, "x").asNumber() == 0 &&
                layout.getProperty(runtime, "y").asNumber() == 0 &&
                layout.getProperty(runtime, "width").asNumber() == width &&
                layout.getProperty(runtime, "height").asNumber() == 20,
            "SVG worklet received incorrect layout bounds");
  }

  void exerciseSvgInterest() {
    require(layer_.itemForTag(static_cast<int>(number("svgTag"))) != nullptr,
            "SVG Rect was not mounted by Fabric");
    waitFor("SVG initial pixels", [&] {
      return pixel(155, 25, QColor(Qt::green)) && pixel(165, 25, QColor(Qt::white));
    });
    flushWorkletQueues("unobserved SVG geometry");
    requireSvgCallbacks(0, 0);

    evaluate("globalThis.__rnqnxRegisterSvgLayout();");
    flushWorkletQueues("late SVG registration");
    requireSvgCallbacks(0, 0);  // Registration does not replay unchanged bounds.
    evaluate("globalThis.__rnqnxSetSvgWidth(20);");
    waitFor("late SVG rendered geometry", [&] { return pixel(165, 25, QColor(Qt::green)); });
    std::cout << "RNQNX_SVG_NATIVE_INTEREST_GEOMETRY width=20\n";
    waitFor("late SVG native subscriber", [&] { return svgCallbackCount() == 1; });
    requireSvgCallbacks(1, 20);

    evaluate("globalThis.__rnqnxUnregisterSvgLayout();");
    flushWorkletQueues("SVG unregistration");
    evaluate("globalThis.__rnqnxSetSvgWidth(30);");
    waitFor("unobserved SVG pixels", [&] { return pixel(175, 25, QColor(Qt::green)); });
    flushWorkletQueues("SVG geometry after unregistration");
    requireSvgCallbacks(1, 20);

    evaluate("globalThis.__rnqnxRegisterSvgLayout();");
    flushWorkletQueues("SVG re-registration");
    requireSvgCallbacks(1, 20);
    evaluate("globalThis.__rnqnxSetSvgWidth(40);");
    waitFor("re-registered SVG rendered geometry", [&] { return pixel(185, 25, QColor(Qt::green)); });
    waitFor("re-registered SVG native subscriber", [&] { return svgCallbackCount() == 2; });
    requireSvgCallbacks(2, 40);
    std::cout << "RNQNX_SVG_NATIVE_INTEREST_OK late remove readd\n";
  }

  // Outlive host-owned event listeners, including surface and runtime teardown.
  std::atomic<int> svgDispatches_{0};
#endif
  QQmlEngine engine_;
  QQuickWindow window_;
  rnqnx::views::QtQuickItemFactory factory_;
  rnqnx::fabric::QtQuickMountingLayer layer_;
  rnqnx::fabric::ShadowViewMutationAdapter adapter_;
  rnqnx::runtime::QtReactHost host_;
  std::shared_ptr<rnqnx::worklets::QtWorkletsPlatform> worklets_;
  std::shared_ptr<rnqnx::gesture::QtGesturePlatform> gestures_;
};

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication application(argc, argv);
  try {
    rnqnx::gesture::registerComponents();
    const std::string bundle = readBundle();
    if (application.arguments().contains(QStringLiteral("--omit-worklets"))) {
      {
        // Same source URL as the real bundle: a stack containing "worklets"
        // must not turn an unrelated application exception into a pass.
        Fixture unrelated(false);
        const bool evaluated = unrelated.load("throw new Error('RNQNX_UNRELATED_STARTUP_FAILURE');");
        require(!evaluated, "unrelated-failure mutation unexpectedly evaluated");
        require(!unrelated.missingBootstrapFailure(evaluated),
                "missing-bootstrap oracle accepted an unrelated application failure");
      }
      Fixture fixture(false);
      const bool evaluated = fixture.load(bundle);
      require(fixture.missingBootstrapFailure(evaluated),
              "missing-bootstrap control did not fail for the expected native dependency: " + fixture.error());
      std::cout << "RNQNX_WORKLETS_MISSING_BOOTSTRAP_REJECTED\n";
      return 0;
    }
    for (int trial = 0; trial < 2; ++trial) {
      Fixture fixture(true);
      const bool evaluated = fixture.load(bundle);
      require(evaluated, "real-package bundle failed: " + fixture.error());
      fixture.start();
      fixture.exercise(trial);
    }
    {
      Fixture fixture(true);
      const bool evaluated = fixture.load(bundle);
      require(evaluated, "teardown bundle failed: " + fixture.error());
      fixture.start();
      fixture.waitFor("queued-work trial mount", [&] { return fixture.flag("ready"); });
      fixture.queueBeforeDestruction();
    }
    QCoreApplication::processEvents();
    std::cout << "RNQNX_WORKLETS_INTEGRATION_OK frames timing pixels pan reload teardown";
#if RNQNX_ENABLE_QT_RNSVG
    std::cout << " svg-native-interest";
#endif
    std::cout << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "RNQNX_WORKLETS_INTEGRATION_FAILED " << error.what() << '\n';
    return 1;
  }
}
