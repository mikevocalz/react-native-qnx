// Compile and run the real consumer/playground entrypoint. The test driver
// supplies Qt window events; it never creates or installs an input dispatcher.
// This is host-window/Fabric/React evidence, not physical HID or QNX proof.
#define main rnqnxPlaygroundMain
#include "../../vnext/probes/playground/main.cpp"
#undef main

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QWheelEvent>

#include <functional>
#include <iostream>

namespace {

bool hasRenderedText(const QQuickItem* item, const QString& text, const QRectF& clip) {
  if (!item->isVisible()) {
    return false;
  }
  const auto visible = item->mapRectToScene(item->boundingRect()).intersected(clip);
  if (!visible.isEmpty() && item->property("text").toString() == text) {
    return true;
  }
  const auto childClip = item->clip() ? visible : clip;
  if (childClip.isEmpty()) {
    return false;
  }
  for (const auto* child : item->childItems()) {
    if (hasRenderedText(child, text, childClip)) {
      return true;
    }
  }
  return false;
}

QQuickItem* findItem(QQuickItem* item, const std::function<bool(QQuickItem*)>& matches) {
  if (matches(item)) return item;
  for (auto* child : item->childItems()) {
    if (auto* found = findItem(child, matches)) return found;
  }
  return nullptr;
}

class ConsumerInputDriver final : public QObject {
 public:
  ConsumerInputDriver() : QObject(QCoreApplication::instance()), timer_(this) {
    elapsed_.start();
    QObject::connect(&timer_, &QTimer::timeout, this, [this] { advance(); });
    timer_.start(10);
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease) {
      // Negative control: block real window delivery without changing the app.
      return true;
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  enum class Step {
    Mount, HoldPadding, FirstPress, HoldText, SecondPress,
    HoldDisabled, DisabledRelease, HoldTab, TabChange, Scrolled, HoldOpenModal,
    ModalOpen, ModalResized, HoldBehindNative, BehindNative,
    HoldBehindModal, BehindModal, HoldModalSwitch, ModalSwitchChanged, HoldCloseModal, ModalClosed,
    HoldReopen, ModalReopened, EscapeClosed, HoldAfterClose, FinalPress, Done
  };

  bool text(const char* expected) const {
    return viewport_ && hasRenderedText(
        viewport_, QLatin1String(expected), viewport_->mapRectToScene(viewport_->boundingRect()));
  }

  void mouse(bool down, const QPointF& point) {
    const auto scenePoint = viewport_->mapToScene(point);
    QMouseEvent event(
        down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
        scenePoint, QPointF(window_->mapToGlobal(scenePoint.toPoint())), Qt::LeftButton,
        down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    event.setTimestamp(static_cast<ulong>(elapsed_.elapsed() + 1));
    QCoreApplication::sendEvent(window_, &event);
  }

  void scroll() {
    const QPointF point = viewport_->mapToScene({500, 140});
    QWheelEvent event(point, QPointF(window_->mapToGlobal(point.toPoint())),
        QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window_, &event);
  }

  bool modalCoversViewport() const {
    for (const auto* child : viewport_->childItems()) {
      if (child->position() == QPointF(0, 0) && child->size() == viewport_->size() &&
          hasRenderedText(child, QStringLiteral("Consumer modal open"),
              viewport_->mapRectToScene(viewport_->boundingRect())) &&
          !hasRenderedText(child, QStringLiteral("Consumer presses: 2"),
              viewport_->mapRectToScene(viewport_->boundingRect()))) return true;
    }
    return false;
  }

  void press(const QPointF& point, Step step) {
    point_ = point;
    mouse(true, point_);
    held_.restart();
    step_ = step;
  }

  bool releaseAfterHold(Step next) {
    if (held_.elapsed() < 180) {
      return false;
    }
    mouse(false, point_);
    held_.restart();
    step_ = next;
    return true;
  }

  void fail(const char* reason) {
    timer_.stop();
    step_ = Step::Done;
    std::cerr << "RNQNX_CONSUMER_INPUT_FAIL " << reason << '\n';
    QCoreApplication::exit(1);
  }

  void advance() {
    if (elapsed_.elapsed() > 10000) {
      const char* reason = "window input did not complete";
      if (step_ == Step::FirstPress) reason = "waiting for first press";
      if (step_ == Step::Scrolled) reason = "waiting for actual scroll";
      if (step_ == Step::ModalOpen) reason = "waiting for visible modal";
      if (step_ == Step::ModalResized) reason = "waiting for resized modal viewport";
      if (step_ == Step::ModalSwitchChanged) reason = "waiting for native switch inside modal";
      if (step_ == Step::ModalClosed) reason = "waiting for modal close";
      if (step_ == Step::EscapeClosed) reason = "waiting for modal Escape callback";
      if (step_ == Step::FinalPress) reason = "waiting for post-modal press";
      fail(reason);
      return;
    }
    switch (step_) {
      case Step::Mount:
        for (auto* candidate : QGuiApplication::allWindows()) {
          if (auto* window = qobject_cast<QQuickWindow*>(candidate); window && window->isVisible()) {
            window_ = window;
            viewport_ = findItem(window_->contentItem(), [](auto* item) {
              return item->objectName() == QStringLiteral("rnqnxDesktopContent");
            });
            if (!viewport_) viewport_ = window_->contentItem();
            break;
          }
        }
        if (!text("Consumer presses: 0") || !text("Consumer page: Components")) {
          return;
        }
        if (qEnvironmentVariableIsSet("RNQNX_CONSUMER_INPUT_INTERCEPT")) {
          window_->installEventFilter(this);
        }
        // Padding hits the production View's unregistered background Rectangle.
        press({28, 28}, Step::HoldPadding);
        break;
      case Step::HoldPadding:
        releaseAfterHold(Step::FirstPress);
        break;
      case Step::FirstPress:
        if (text("Consumer presses: 1")) {
          // This point hits the Text descendant inside the same Pressable.
          press({70, 50}, Step::HoldText);
        }
        break;
      case Step::HoldText:
        releaseAfterHold(Step::SecondPress);
        break;
      case Step::SecondPress:
        if (text("Consumer presses: 2")) {
          press({28, 128}, Step::HoldDisabled);
        }
        break;
      case Step::HoldDisabled:
        releaseAfterHold(Step::DisabledRelease);
        break;
      case Step::DisabledRelease:
        if (held_.elapsed() < 220) {
          return;
        }
        if (!text("Disabled presses: 0") || !text("Consumer presses: 2")) {
          fail("disabled button activated or repeat press changed");
          return;
        }
        press({228, 28}, Step::HoldTab);
        break;
      case Step::HoldTab:
        releaseAfterHold(Step::TabChange);
        break;
      case Step::TabChange:
        if (text("Consumer page: Documents")) {
          scrollItem_ = findItem(viewport_, [](auto* item) {
            return item->property("contentY").isValid() &&
                item->property("contentHeight").toDouble() > item->height();
          });
          if (!scrollItem_) return;
          scroll();
          step_ = Step::Scrolled;
        }
        break;
      case Step::Scrolled:
        if (scrollItem_->property("contentY").toDouble() > 10 &&
            !scrollItem_->property("moving").toBool()) {
          press({228, 128}, Step::HoldOpenModal);
        }
        break;
      case Step::HoldOpenModal:
        releaseAfterHold(Step::ModalOpen);
        break;
      case Step::ModalOpen:
        if (text("Consumer modal open") && modalCoversViewport()) {
          window_->resize(window_->width() + 80, window_->height() + 40);
          step_ = Step::ModalResized;
        }
        break;
      case Step::ModalResized:
        if (modalCoversViewport()) {
          modalScrollOffset_ = scrollItem_->property("contentY").toDouble();
          scroll();
          press({28, 328}, Step::HoldBehindNative);
        }
        break;
      case Step::HoldBehindNative:
        releaseAfterHold(Step::BehindNative);
        break;
      case Step::BehindNative:
        if (held_.elapsed() < 220) return;
        if (!text("Native switch: off") ||
            scrollItem_->property("contentY").toDouble() != modalScrollOffset_ ||
            scrollItem_->property("moving").toBool()) {
          fail("modal leaked input to native background controls");
          return;
        }
        // Fabric responder delivery also must stay above the background.
        press({28, 28}, Step::HoldBehindModal);
        break;
      case Step::HoldBehindModal:
        releaseAfterHold(Step::BehindModal);
        break;
      case Step::BehindModal:
        if (held_.elapsed() < 220) return;
        if (!text("Consumer presses: 2")) {
          fail("modal leaked input to underlying button");
          return;
        }
        press({(viewport_->width() - 280) / 2 + 28,
               (viewport_->height() - 180) / 2 + 60}, Step::HoldModalSwitch);
        break;
      case Step::HoldModalSwitch:
        releaseAfterHold(Step::ModalSwitchChanged);
        break;
      case Step::ModalSwitchChanged:
        if (text("Modal switch: on")) {
          press({(viewport_->width() - 280) / 2 + 28,
                 (viewport_->height() - 180) / 2 + 98}, Step::HoldCloseModal);
        }
        break;
      case Step::HoldCloseModal:
        releaseAfterHold(Step::ModalClosed);
        break;
      case Step::ModalClosed:
        if (!text("Consumer modal open")) {
          press({228, 128}, Step::HoldReopen);
        }
        break;
      case Step::HoldReopen:
        releaseAfterHold(Step::ModalReopened);
        break;
      case Step::ModalReopened:
        if (text("Consumer modal open") && modalCoversViewport()) {
          QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
          QCoreApplication::sendEvent(window_, &event);
          step_ = Step::EscapeClosed;
        }
        break;
      case Step::EscapeClosed:
        if (!text("Consumer modal open")) {
          press({28, 28}, Step::HoldAfterClose);
        }
        break;
      case Step::HoldAfterClose:
        releaseAfterHold(Step::FinalPress);
        break;
      case Step::FinalPress:
        if (text("Consumer presses: 3") && text("Consumer page: Documents") &&
            text("Disabled presses: 0")) {
          timer_.stop();
          step_ = Step::Done;
          std::cout << "RNQNX_CONSUMER_INPUT_OK window padding text repeat disabled tab modal after-close scrolled-portal resized native-barrier escape lifecycle="
                    << RNQNX_ENABLE_INPUT_LIFECYCLE << '\n';
          QCoreApplication::exit(0);
        }
        break;
      case Step::Done:
        break;
    }
  }

  QTimer timer_;
  QElapsedTimer elapsed_;
  QElapsedTimer held_;
  QPointer<QQuickWindow> window_;
  QPointer<QQuickItem> viewport_;
  QPointer<QQuickItem> scrollItem_;
  QPointF point_;
  double modalScrollOffset_{0};
  Step step_{Step::Mount};
};

void startConsumerInputDriver() {
  // Called by the real QGuiApplication constructor, before its main loop starts.
  QTimer::singleShot(0, [] { new ConsumerInputDriver(); });
}
Q_COREAPP_STARTUP_FUNCTION(startConsumerInputDriver)

}  // namespace

int main(int argc, char** argv) {
  return rnqnxPlaygroundMain(argc, argv);
}
