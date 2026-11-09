// Host evidence: the actual gallery bundle and production startup.
// No JavaScript store access, handler calls, or test-installed input dispatcher.
#define main rnqnxGalleryProductionMain
#include "../../vnext/probes/playground/main.cpp"
#undef main

#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QScopedValueRollback>
#include <QWheelEvent>

#include <functional>
#include <iostream>
#include <vector>

namespace gallery_smoke {

enum class Kind { Text, ClickText, Switch, Absent, Escape, Snapshot, MinimumResize, MinimumAdaptive, WideResize, ViewportWidth };
struct Action {
  Kind kind;
  const char* target;
  bool scroll{true};
};

QQuickItem* findAny(QQuickItem* item,
    const std::function<bool(QQuickItem*)>& matches) {
  if (matches(item)) return item;
  for (auto* child : item->childItems()) {
    if (auto* found = findAny(child, matches)) return found;
  }
  return nullptr;
}

QRectF visibleRect(QQuickItem* item, QQuickItem* viewport) {
  if (!item->isVisible() || item->width() <= 0 || item->height() <= 0) return {};
  QRectF result = item->mapRectToScene(item->boundingRect());
  for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
    if (!parent->isVisible()) return {};
    if (parent->clip() || parent == viewport)
      result = result.intersected(parent->mapRectToScene(parent->boundingRect()));
  }
  return result.intersected(viewport->mapRectToScene(viewport->boundingRect()));
}

bool fullyVisible(QQuickItem* item, QQuickItem* viewport) {
  const auto full = item->mapRectToScene(item->boundingRect());
  const auto visible = visibleRect(item, viewport);
  return !visible.isEmpty() && visible.width() >= full.width() - 1 &&
      visible.height() >= full.height() - 1;
}

class GalleryDriver final : public QObject {
 public:
  GalleryDriver() : QObject(QCoreApplication::instance()), timer_(this) {
    elapsed_.start();
    actionTime_.start();
    lastWheel_.start();
    output_ = qEnvironmentVariable("RNQNX_GALLERY_SMOKE_ARTIFACTS");
    if (output_.isEmpty()) output_ = QDir::currentPath();
    QDir().mkpath(output_);
    actions_ = {
        {Kind::Text, "Component gallery", false},
        {Kind::Text, "React Native components, native Qt Quick pixels"},
        {Kind::Snapshot, "01-components-initial"},
        {Kind::Text, "PNG decoded by the image backend"},
        {Kind::ClickText, "Press count: 0"},
        {Kind::Text, "Press count: 1"},
        {Kind::Text, "Switch OFF"},
        {Kind::Switch, "gallery switch on"},
        {Kind::Text, "Switch ON"},
        {Kind::ClickText, "Documents", false},
        {Kind::Text, "LIST-001 / 1,000 synthetic documents", false},
        {Kind::ClickText, "Document 0001"},
        {Kind::Text, "Selected: document-0001", false},
        {Kind::Snapshot, "02-documents"},
        {Kind::ClickText, "Sections", false},
        {Kind::Text, "SectionList", false},
        {Kind::Text, "Selected: document-0001", false},
        {Kind::Text, "Layout and content"},
        {Kind::Snapshot, "03-sections"},
        {Kind::ClickText, "Controls", false},
        {Kind::Text, "INPUT-001 / controlled inputs"},
        {Kind::Text, "Switch ON"},
        {Kind::ClickText, "Press count: 1"},
        {Kind::Text, "Press count: 2"},
        {Kind::Snapshot, "04-controls"},
        {Kind::ClickText, "Adaptive", false},
        {Kind::Text, "LAYOUT-001 / live window layout", false},
        {Kind::Text, "Selected document-0001", false},
        {Kind::Text, "Inspector"},
        {Kind::Snapshot, "05-adaptive"},
        {Kind::ClickText, "Limits", false},
        {Kind::Text, "Evidence and unsupported work"},
        {Kind::Snapshot, "06-limits"},
        {Kind::ClickText, "Components", false},
        {Kind::Text, "React Native components, native Qt Quick pixels"},
        {Kind::Text, "Press count: 2"},
        {Kind::Text, "Switch ON"},
        {Kind::ClickText, "Open modal"},
        {Kind::Text, "React Native Modal", false},
        {Kind::Text, "Press count remains 2", false},
        {Kind::Snapshot, "07-modal"},
        {Kind::ClickText, "Documents", false},
        {Kind::Text, "React Native Modal", false},
        {Kind::Absent, "LIST-001 / 1,000 synthetic documents", false},
        {Kind::ClickText, "Close modal", false},
        {Kind::Absent, "React Native Modal", false},
        {Kind::ClickText, "Open modal"},
        {Kind::Text, "React Native Modal", false},
        {Kind::Escape, "gallery Modal Escape"},
        {Kind::Absent, "React Native Modal", false},
        {Kind::Text, "Switch ON"},
        {Kind::ClickText, "Press count: 2"},
        {Kind::Text, "Press count: 3"},
        {Kind::Snapshot, "08-components-retained-state"},
    };
    if (QCoreApplication::arguments().contains(QStringLiteral("--desktop"))) {
      actions_.insert(actions_.end(), {
          {Kind::ClickText, "Adaptive", false},
          {Kind::Text, "LAYOUT-001 / live window layout"},
          {Kind::MinimumResize, "native minimum window size"},
          {Kind::Text, "LAYOUT-001 / live window layout"},
          {Kind::MinimumAdaptive, "usable minimum viewport and complete Inspector labels"},
          {Kind::Snapshot, "09-adaptive-minimum"},
          {Kind::WideResize, "wide desktop window size"},
          {Kind::Text, "Wide two-column mode"},
          {Kind::ViewportWidth, "React layout follows the actual desktop client width"},
          {Kind::ClickText, "Components", false},
          {Kind::Text, "React Native components, native Qt Quick pixels"},
          {Kind::Snapshot, "10-desktop-gallery"},
      });
    }
    QObject::connect(&timer_, &QTimer::timeout, this, [this] { advance(); });
    timer_.start(10);
  }

 private:
  QQuickItem* textItem(const char* target, bool visible) const {
    if (!viewport_) return nullptr;
    return findAny(viewport_, [&](auto* item) {
      return item->property("text").toString() == QLatin1String(target) &&
          (!visible || fullyVisible(item, viewport_));
    });
  }

  void recordItems(QQuickItem* item, QJsonArray& result) const {
    const QString text = item->property("text").toString();
    if (!text.isEmpty()) {
      const QRectF rect = item->mapRectToScene(item->boundingRect());
      result.append(QJsonObject{{"text", text}, {"fullyVisible", fullyVisible(item, viewport_)},
          {"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}});
    }
    for (auto* child : item->childItems()) recordItems(child, result);
  }

  bool snapshot(const QString& name) {
    if (!window_ || !viewport_) return false;
    const QImage frame = window_->grabWindow();
    if (frame.isNull() || !frame.save(output_ + '/' + name + QStringLiteral(".png"))) return false;
    QJsonArray labels;
    recordItems(viewport_, labels);
    QFile record(output_ + '/' + name + QStringLiteral(".json"));
    if (!record.open(QIODevice::WriteOnly)) return false;
    if (record.write(QJsonDocument(QJsonObject{{"windowWidth", window_->width()},
          {"windowHeight", window_->height()}, {"clientWidth", viewport_->width()},
          {"clientHeight", viewport_->height()}, {"labels", labels}}).toJson()) < 0) return false;
    if (name == QStringLiteral("05-adaptive")) {
      auto* inspector = textItem("Inspector", false);
      auto* footer = findAny(viewport_, [](auto* item) {
        return item->property("text").toString().startsWith(QStringLiteral("Experimental component fixture"));
      });
      if (inspector && footer && inspector->mapRectToScene(inspector->boundingRect())
          .intersects(footer->mapRectToScene(footer->boundingRect()))) {
        std::cerr << "RNQNX_GALLERY_LAYOUT_FAIL Adaptive Inspector overlaps footer" << std::endl;
      }
    }
    return true;
  }

  void fail(const QString& reason) {
    timer_.stop();
    snapshot(QStringLiteral("failure"));
    std::cerr << "RNQNX_GALLERY_SMOKE_FAIL step=" << index_ << " reason="
              << reason.toStdString() << std::endl;
    QCoreApplication::exit(1);
  }

  void complete() {
    std::cout << "RNQNX_GALLERY_STEP " << index_ << ' ' << actions_[index_].target
              << " elapsed_ms=" << elapsed_.elapsed() << std::endl;
    ++index_;
    holding_ = false;
    actionTime_.restart();
  }

  void mouse(bool pressed) {
    QMouseEvent event(pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
        point_, QPointF(window_->mapToGlobal(point_.toPoint())), Qt::LeftButton,
        pressed ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    event.setTimestamp(static_cast<quint64>(elapsed_.elapsed() + 1));
    QCoreApplication::sendEvent(window_, &event);
  }

  void seek(const char* target) {
    if (lastWheel_.elapsed() < 200) return;
    auto* targetItem = textItem(target, false);
    // Every sought gallery control is already mounted. Do not scroll in
    // response to an expected state label that has not actually appeared.
    if (!targetItem) return;
    QQuickItem* scroll = nullptr;
    double largest = 0;
    findAny(viewport_, [&](auto* item) {
      const auto rect = visibleRect(item, viewport_);
      if (item->property("contentY").isValid() &&
          item->property("contentHeight").toDouble() > item->height() + 1 &&
          rect.width() * rect.height() > largest) {
        scroll = item;
        largest = rect.width() * rect.height();
      }
      return false;
    });
    if (!scroll || scroll->property("moving").toBool()) return;
    const QRectF rect = visibleRect(scroll, viewport_);
    const bool above = targetItem &&
        targetItem->mapRectToScene(targetItem->boundingRect()).top() < rect.top();
    const QPointF point = rect.center();
    QWheelEvent event(point, QPointF(window_->mapToGlobal(point.toPoint())),
        QPoint(), QPoint(0, above ? 120 : -120), Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    event.setTimestamp(static_cast<quint64>(elapsed_.elapsed() + 1));
    QCoreApplication::sendEvent(window_, &event);
    lastWheel_.restart();
  }

  void advance() {
    if (advancing_) return;
    QScopedValueRollback<bool> guard(advancing_, true);
    if (elapsed_.elapsed() > 60000) { fail(QStringLiteral("overall timeout")); return; }
    if (!window_) {
      for (auto* candidate : QGuiApplication::allWindows()) {
        auto* quick = qobject_cast<QQuickWindow*>(candidate);
        if (quick && quick->isVisible()) {
          window_ = quick;
          viewport_ = findAny(window_->contentItem(), [](auto* item) {
            return item->objectName() == QStringLiteral("rnqnxDesktopContent");
          });
          if (!viewport_) viewport_ = window_->contentItem();
          window_->requestActivate();
          break;
        }
      }
      if (!window_) return;
    }
    if (index_ == actions_.size()) {
      timer_.stop();
      std::cout << "RNQNX_GALLERY_INTERACTIONS_COMPLETE tabs=Components,Documents,Sections,Controls,Adaptive,Limits,Components"
                   " presses=3 switch=on selected=document-0001 modal=button-and-Escape"
                   " lifecycle=" << RNQNX_ENABLE_INPUT_LIFECYCLE
                << " client=" << viewport_->width() << 'x' << viewport_->height() << std::endl;
      QCoreApplication::exit(0);
      return;
    }
    const auto& action = actions_[index_];
    if (actionTime_.elapsed() > 10000) {
      fail(QStringLiteral("could not reach/complete: ") + QLatin1String(action.target));
      return;
    }
    if (holding_) {
      if (held_.elapsed() >= 180) { mouse(false); complete(); }
      return;
    }
    if (action.kind == Kind::Snapshot) {
      if (!snapshot(QLatin1String(action.target))) {
        fail(QStringLiteral("could not capture gallery evidence"));
        return;
      }
      complete();
      return;
    }
    if (action.kind == Kind::MinimumResize) {
      window_->resize(window_->minimumSize());
      std::cout << "RNQNX_GALLERY_MINIMUM window=" << window_->width() << 'x'
                << window_->height() << " client=" << viewport_->width() << 'x'
                << viewport_->height() << std::endl;
      complete();
      return;
    }
    if (action.kind == Kind::WideResize) {
      window_->resize(1100, 760);
      complete();
      return;
    }
    if (action.kind == Kind::MinimumAdaptive) {
      // A label that can be glimpsed one line at a time is not a usable page.
      // Keep every tab available and enough real scroll viewport for the
      // Inspector heading and its detail to be readable together.
      for (const char* tab : {"Components", "Documents", "Sections", "Controls", "Adaptive", "Limits"}) {
        if (!textItem(tab, true)) return;
      }
      auto* inspector = textItem("Inspector", false);
      if (!inspector) return;
      auto* scroll = inspector->parentItem();
      while (scroll && !scroll->property("contentY").isValid()) scroll = scroll->parentItem();
      if (!scroll || visibleRect(scroll, viewport_).height() < 100 ||
          scroll->property("moving").toBool()) return;
      if (!textItem("Narrow stacked mode", true)) { seek("Narrow stacked mode"); return; }
      if (!fullyVisible(inspector, viewport_)) { seek("Inspector"); return; }
      std::cout << "RNQNX_GALLERY_MINIMUM_USABLE page_height="
                << visibleRect(scroll, viewport_).height()
                << " tabs=6 inspector_and_detail=visible" << std::endl;
      complete();
      return;
    }
    if (action.kind == Kind::ViewportWidth) {
      const QByteArray expected = (QStringLiteral("Observed width: ") +
          QString::number(qRound(viewport_->width()))).toUtf8();
      if (textItem(expected.constData(), true)) complete();
      else seek(expected.constData());
      return;
    }
    if (action.kind == Kind::Escape) {
      QKeyEvent press(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
      QKeyEvent release(QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
      QCoreApplication::sendEvent(window_, &press);
      QCoreApplication::sendEvent(window_, &release);
      complete();
      return;
    }
    if (action.kind == Kind::Absent) {
      if (!textItem(action.target, true) && actionTime_.elapsed() >= 250) complete();
      return;
    }
    QQuickItem* item = action.kind == Kind::Switch
        ? findAny(viewport_, [&](auto* candidate) {
            return candidate->property("trackOnColor").isValid() &&
                fullyVisible(candidate, viewport_);
          })
        : textItem(action.target, true);
    if (!item) {
      if (action.scroll && action.kind != Kind::Switch) seek(action.target);
      return;
    }
    for (auto* ancestor = item->parentItem(); ancestor; ancestor = ancestor->parentItem()) {
      if (ancestor->property("moving").toBool()) return;
    }
    if (action.kind == Kind::Text) { complete(); return; }
    point_ = visibleRect(item, viewport_).center();
    std::cout << "RNQNX_GALLERY_CLICK " << action.target << " scene=" << point_.x()
              << ',' << point_.y() << std::endl;
    mouse(true);
    held_.restart();
    holding_ = true;
  }

  QTimer timer_;
  QElapsedTimer elapsed_;
  QElapsedTimer actionTime_;
  QElapsedTimer held_;
  QElapsedTimer lastWheel_;
  QPointer<QQuickWindow> window_;
  QPointer<QQuickItem> viewport_;
  QString output_;
  std::vector<Action> actions_;
  std::size_t index_{0};
  QPointF point_;
  bool holding_{false};
  bool advancing_{false};
};

void startDriver() { QTimer::singleShot(0, [] { new GalleryDriver(); }); }
Q_COREAPP_STARTUP_FUNCTION(startDriver)

}  // namespace gallery_smoke

int main(int argc, char** argv) { return rnqnxGalleryProductionMain(argc, argv); }
