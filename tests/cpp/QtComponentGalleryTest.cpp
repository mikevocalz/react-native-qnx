// Drives the shipped gallery through the production consumer entrypoint.
// Qt events here are synthetic host input, not physical QNX HID evidence.
#define main rnqnxPlaygroundMain
#include "../../vnext/probes/playground/main.cpp"
#undef main

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QScopedValueRollback>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Route {
  const char* label;
  const char* testId;
};
constexpr Route kRoutes[] = {
    {"Components", "gallery-route-gallery"},
    {"Documents", "gallery-route-list"},
    {"Sections", "gallery-route-sections"},
    {"Controls", "gallery-route-controls"},
    {"Adaptive", "gallery-route-adaptive"},
    {"Limits", "gallery-route-limits"},
};

QQuickItem* findItem(QQuickItem* root, const std::function<bool(QQuickItem*)>& match) {
  if (!root) return nullptr;
  if (match(root)) return root;
  for (auto* child : root->childItems()) {
    if (auto* found = findItem(child, match)) return found;
  }
  return nullptr;
}

bool presented(QQuickItem* item) {
  if (!item || !item->isVisible() || !item->isEnabled() || item->opacity() <= 0) return false;
  for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
    if (!parent->isVisible() || !parent->isEnabled() || parent->opacity() <= 0) return false;
  }
  return true;
}

QRectF visibleRect(QQuickItem* item) {
  if (!presented(item)) return {};
  QRectF result = item->mapRectToScene(item->boundingRect());
  for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
    if (parent->clip()) result = result.intersected(parent->mapRectToScene(parent->boundingRect()));
  }
  return result;
}

void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

class GalleryDriver final : public QObject {
 public:
  GalleryDriver() : QObject(QCoreApplication::instance()), timer_(this) {
    defineSteps();
    phaseTime_.start();
    held_.start();
    inputClock_.start();
    lastWheel_.start();
    QObject::connect(&timer_, &QTimer::timeout, this, [this] { advance(); });
    timer_.start(16);
  }

 private:
  using Action = std::function<bool()>;
  struct Step { const char* name; Action action; };

  QQuickItem* id(const char* value) const {
    return viewport_ ? findItem(viewport_, [value](auto* item) {
      return item->property("testID").toString() == QLatin1String(value);
    }) : nullptr;
  }

  QQuickItem* label(const char* value) const {
    return viewport_ ? findItem(viewport_, [value](auto* item) {
      return item->property("accessibilityLabel").toString() == QLatin1String(value) &&
          !visibleRect(item).isEmpty();
    }) : nullptr;
  }

  QQuickItem* availableLabel(const char* value) const {
    return findItem(viewport_, [value](auto* item) {
      return presented(item) && item->property("accessibilityLabel").toString() == QLatin1String(value);
    });
  }

  QQuickItem* route(const char* testId) const {
    auto* item = id(testId);
    return presented(item) && !visibleRect(item).isEmpty() ? item : nullptr;
  }

  bool modalVisible() const {
    return findItem(viewport_, [](auto* item) {
      return presented(item) && item->property("rnqnxViewportModal").toBool();
    }) != nullptr;
  }

  bool fullyVisible(QQuickItem* item) const {
    if (!item || !viewport_) return false;
    const auto full = item->mapRectToScene(item->boundingRect());
    const auto visible = visibleRect(item).intersected(viewport_->mapRectToScene(viewport_->boundingRect()));
    return !visible.isEmpty() && visible.width() >= full.width() - 0.6 &&
        visible.height() >= full.height() - 0.6;
  }

  QQuickItem* textItem(QQuickItem* root, const QString& value) const {
    return findItem(root, [&value](auto* item) {
      return presented(item) && item->property("text").toString() == value;
    });
  }

  bool text(const QString& value) const {
    return viewport_ && findItem(viewport_, [&value](auto* item) {
      return item->property("text").toString() == value && !visibleRect(item).isEmpty();
    });
  }

  bool observedClientSize(QQuickItem* active) const {
    const auto prefix = QStringLiteral("Window: ") + QString::number(qRound(viewport_->width())) +
        QStringLiteral(" × ") + QString::number(qRound(viewport_->height())) + QStringLiteral(" logical units");
    return findItem(active, [&](auto* item) {
      return item->property("text").toString().startsWith(prefix) && fullyVisible(item);
    }) != nullptr;
  }

  void key(int code, const QString& value = {}, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent down(QEvent::KeyPress, code, modifiers, value);
    QKeyEvent up(QEvent::KeyRelease, code, modifiers, value);
    QCoreApplication::sendEvent(window_, &down);
    QCoreApplication::sendEvent(window_, &up);
  }

  void mouse(bool down) {
    QMouseEvent event(down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
        point_, QPointF(window_->mapToGlobal(point_.toPoint())), Qt::LeftButton,
        down ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    event.setTimestamp(static_cast<ulong>(inputClock_.elapsed() + 1));
    QCoreApplication::sendEvent(window_, &event);
  }

  bool beginPress(QQuickItem* target) {
    if (!target || !reveal(target)) return false;
    const auto visible = visibleRect(target).intersected(viewport_->mapRectToScene(viewport_->boundingRect()));
    if (visible.width() < 12 || visible.height() < 12) return false;
    point_ = visible.center();
    mouse(true);
    held_.restart();
    return true;
  }

  void addClick(const char* name, const std::function<QQuickItem*()>& target) {
    steps_.push_back({name, [this, target] { return beginPress(target()); }});
    steps_.push_back({"release pointer", [this] {
      if (held_.elapsed() < 180) return false;
      mouse(false);
      return true;
    }});
  }

  void resizeClient(int width, int height) {
    const QSize chrome(window_->width() - qRound(viewport_->width()),
        window_->height() - qRound(viewport_->height()));
    window_->resize(width + chrome.width(), height + chrome.height());
    requestedSize_ = QSizeF(width, height);
  }

  bool sized() const {
    auto* shell = id("adaptive-shell");
    return shell && std::abs(shell->width() - requestedSize_.width()) < 0.6 &&
        std::abs(shell->height() - requestedSize_.height()) < 0.6;
  }

  bool followsClient() const {
    auto* shell = id("adaptive-shell");
    return shell && viewport_->width() > 0 && viewport_->height() > 0 &&
        std::abs(shell->width() - viewport_->width()) < 0.6 &&
        std::abs(shell->height() - viewport_->height()) < 0.6;
  }

  void checkState(bool focused = false) const {
    require(field_ && field_ == id("gallery-text-input"), "Resize/navigation replaced TextInput");
    require(press_ && press_ == id("gallery-press"), "Resize/navigation replaced Pressable");
    require(field_->property("text").toString() == editedText_, "Resize/navigation lost edited text");
    require(press_->property("accessibilityLabel").toString() ==
            QStringLiteral("Press count: ") + QString::number(expectedPresses_),
        "Resize/navigation lost React counter state");
    require(id("gallery-switch") && id("gallery-switch")->property("value").toBool(),
        "Resize/navigation lost the switch state");
    require(editor_ && editor_->property("cursorPosition").toInt() == caret_, "Resize/navigation moved the caret");
    if (focused) require(editor_->hasActiveFocus(), "Window resize stole text-input focus");
  }

  int checkGrid() const {
    auto* grid = id("adaptive-grid");
    require(grid && grid->width() > 0, "Gallery grid has no usable width");
    std::vector<qreal> columns;
    int cards = 0;
    findItem(grid, [&](auto* item) {
      if (!item->property("testID").toString().startsWith(QStringLiteral("example-card-"))) return false;
      ++cards;
      const auto bounds = item->mapRectToItem(grid, item->boundingRect());
      require(bounds.left() >= -0.6 && bounds.right() <= grid->width() + 0.6,
          "Card overflowed the measured main content width");
      require(bounds.width() > 0 && bounds.height() > 0, "Card collapsed to zero");
      if (std::none_of(columns.begin(), columns.end(), [&](qreal x) { return std::abs(x - bounds.left()) < 1; })) {
        columns.push_back(bounds.left());
      }
      return false;
    });
    require(cards == 6, "Gallery did not retain all six cards");
    return static_cast<int>(columns.size());
  }

  void recordItems(QQuickItem* item, QJsonArray& labels) const {
    const auto content = item->property("text").toString();
    if (!content.isEmpty()) {
      const auto bounds = item->mapRectToScene(item->boundingRect());
      labels.append(QJsonObject{{"text", content}, {"fullyVisible", fullyVisible(item)},
          {"x", bounds.x()}, {"y", bounds.y()}, {"width", bounds.width()}, {"height", bounds.height()}});
    }
    for (auto* child : item->childItems()) recordItems(child, labels);
  }

  void capture(const char* name) const {
    const auto directory = qEnvironmentVariable("RNQNX_GALLERY_SMOKE_ARTIFACTS", QDir::currentPath());
    require(QDir().mkpath(directory), "Could not create screenshot directory");
    const auto frame = window_->grabWindow();
    require(!frame.isNull() && frame.save(directory + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".png")),
        "Could not capture the real gallery frame");
    QJsonArray labels;
    recordItems(viewport_, labels);
    const auto record = QJsonDocument(QJsonObject{{"windowWidth", window_->width()},
        {"windowHeight", window_->height()}, {"clientWidth", viewport_->width()},
        {"clientHeight", viewport_->height()}, {"labels", labels}}).toJson();
    QFile file(directory + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".json"));
    if (!file.open(QIODevice::WriteOnly)) throw std::runtime_error("Could not open screenshot geometry record");
    if (file.write(record) != record.size() || !file.flush()) {
      throw std::runtime_error("Could not write screenshot geometry record");
    }
  }

  void wheel(QQuickItem* target, int direction) {
    const auto point = visibleRect(target).center();
    QWheelEvent event(point, QPointF(window_->mapToGlobal(point.toPoint())),
        QPoint(), QPoint(0, direction * 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    event.setTimestamp(static_cast<ulong>(inputClock_.elapsed() + 1));
    QCoreApplication::sendEvent(window_, &event);
    lastWheel_.restart();
  }

  bool reveal(QQuickItem* item, QQuickItem* companion = nullptr) {
    if (!presented(item) || (companion && !presented(companion))) return false;
    auto* scroll = item->parentItem();
    while (scroll && !scroll->property("contentY").isValid()) scroll = scroll->parentItem();
    if (scroll && scroll->property("moving").toBool()) return false;
    if (fullyVisible(item) && (!companion || fullyVisible(companion))) return true;
    if (!scroll || lastWheel_.elapsed() < 180) return false;
    const auto visible = visibleRect(scroll);
    if (visible.isEmpty()) return false;
    auto wanted = item->mapRectToScene(item->boundingRect());
    if (companion) wanted = wanted.united(companion->mapRectToScene(companion->boundingRect()));
    wheel(scroll, wanted.top() < visible.top() ? 1 : -1);
    return false;
  }

  void defineSteps() {
    steps_.push_back({"mount production gallery", [this] {
      for (auto* candidate : QGuiApplication::allWindows()) {
        if (auto* window = qobject_cast<QQuickWindow*>(candidate); window && window->isVisible()) {
          window_ = window;
          viewport_ = findItem(window->contentItem(), [](auto* item) {
            return item->objectName() == QStringLiteral("rnqnxDesktopContent");
          });
          if (!viewport_) viewport_ = window->contentItem();
          break;
        }
      }
      if (!id("adaptive-shell") || !id("gallery-text-input")) return false;
      window_->requestActivate();
      resizeClient(1280, 900);
      return true;
    }});
    steps_.push_back({"wide content layout", [this] {
      if (!sized() || !label("Components") || !label("Press count: 0")) return false;
      if (!text(QStringLiteral("PNG decoded by the image backend"))) return false;
      if (checkGrid() < 2) return false;
      press_ = id("gallery-press");
      field_ = id("gallery-text-input");
      editor_ = field_->findChild<QQuickItem*>(QStringLiteral("qnx-text-input"));
      require(editor_, "The gallery TextInput has no native editor");
      capture("01-components-initial");
      return true;
    }});
    addClick("press native gallery action", [this] { return press_.data(); });
    steps_.push_back({"React press state", [this] { return label("Press count: 1") != nullptr; }});
    addClick("toggle the native switch", [this] { return id("gallery-switch"); });
    steps_.push_back({"React switch state", [this] { return text(QStringLiteral("Switch ON")); }});
    addClick("focus native editor", [this] { return field_.data(); });
    steps_.push_back({"edit through window key events", [this] {
      if (!editor_->hasActiveFocus()) return false;
      key(Qt::Key_End);
      key(0, QStringLiteral(" adaptive"));
      key(Qt::Key_Left);
      return true;
    }});
    steps_.push_back({"controlled text acknowledgement", [this] {
      editedText_ = QStringLiteral("QNX input sample adaptive");
      if (field_->property("text").toString() != editedText_ ||
          !text(QStringLiteral("React value: ") + editedText_)) return false;
      caret_ = editor_->property("cursorPosition").toInt();
      require(caret_ == editedText_.size() - 1, "Native caret did not follow Left key");
      return true;
    }});
    std::vector<QSize> sizes{QSize(800, 800), QSize(1024, 768), QSize(1920, 1080), QSize(480, 700)};
    // The desktop frame intentionally has a 480-unit minimum. The ordinary
    // host also qualifies a narrower client, without overriding that policy.
    if (!QCoreApplication::arguments().contains(QStringLiteral("--desktop"))) sizes.emplace_back(360, 700);
    for (const auto size : sizes) {
      steps_.push_back({"resize client without remount", [this, size] {
        resizeClient(size.width(), size.height()); return true;
      }});
      steps_.push_back({"state and geometry after resize", [this, size] {
        if (!sized()) return false;
        const int columns = checkGrid();
        if ((size.width() <= 480 && columns != 1) || (size.width() == 1920 && columns != 3)) return false;
        checkState(true);
        std::cout << "RNQNX_ADAPTIVE_SIZE " << size.width() << 'x' << size.height()
                  << " columns=" << columns << " scale=" << window_->devicePixelRatio() << '\n';
        if (size.width() == 1920) capture("gallery-expanded");
        if (size.width() == 480) capture("gallery-compact");
        if (size.width() == 360) capture("gallery-narrow");
        return true;
      }});
    }
    addClick("open compact navigation", [this] { return label("Browse sections"); });
    steps_.push_back({"all six destinations in compact navigation", [this] {
      for (const auto* destination : {"Components", "Documents", "Sections", "Controls", "Adaptive", "Limits"}) {
        if (!label(destination)) return false;
      }
      key(Qt::Key_Escape);
      return true;
    }});
    steps_.push_back({"navigation dismissal restores focus", [this] {
      auto* menu = label("Browse sections");
      if (modalVisible()) return false;
      require(menu && menu->hasActiveFocus(), "Closing navigation did not restore its trigger focus");
      checkState();
      resizeClient(1200, 400);
      return true;
    }});
    steps_.push_back({"short wide window remains usable", [this] {
      if (!sized() || !label("Limits")) return false;
      checkGrid();
      capture("gallery-short");
      resizeClient(1280, 900);
      return true;
    }});
    steps_.push_back({"return to wide client", [this] { return sized() && label("Open modal"); }});
    addClick("open native overlay", [this] { return label("Open modal"); });
    steps_.push_back({"visible bounded dialog", [this] {
      if (!label("Close modal") || !text(QStringLiteral("React Native Modal"))) return false;
      resizeClient(1200, 400);
      return true;
    }});
    steps_.push_back({"short dialog geometry settles", [this] { return sized() && label("Close modal"); }});
    addClick("attempt background navigation under modal", [this] { return label("Documents"); });
    steps_.push_back({"dialog adapts to short viewport", [this] {
      auto* close = label("Close modal");
      if (!sized() || !close || !close->hasActiveFocus()) return false;
      require(route("gallery-route-gallery") && !route("gallery-route-list"),
          "Dialog pointer input activated background navigation");
      key(Qt::Key_Tab);
      require(close->hasActiveFocus(), "Tab escaped the dialog's only action");
      key(Qt::Key_Backtab, {}, Qt::ShiftModifier);
      require(close->hasActiveFocus(), "Backtab escaped the dialog's only action");
      key(0, QStringLiteral("behind modal"));
      require(field_->property("text").toString() == editedText_, "Dialog keyboard input reached the background editor");
      capture("gallery-dialog-short");
      capture("07-modal");
      key(Qt::Key_Escape);
      return true;
    }});
    steps_.push_back({"dialog dismissal and state", [this] {
      if (label("Close modal")) return false;
      auto* opener = label("Open modal");
      // The opener may be below the short viewport; its native identity still
      // receives focus. Find it without requiring a visible hit rectangle.
      if (!opener) opener = findItem(viewport_, [](auto* item) {
        return item->property("accessibilityLabel").toString() == QStringLiteral("Open modal");
      });
      require(opener && opener->hasActiveFocus(), "Closing dialog did not restore its opener focus");
      checkState();
      resizeClient(1280, 900);
      return true;
    }});
    steps_.push_back({"wide dialog button dismissal ready", [this] { return sized() && availableLabel("Open modal"); }});
    addClick("open overlay for close button", [this] { return availableLabel("Open modal"); });
    steps_.push_back({"close button is mounted", [this] { return label("Close modal") != nullptr; }});
    addClick("close overlay through native button", [this] { return label("Close modal"); });
    steps_.push_back({"button dismissal retains state", [this] {
      if (modalVisible()) return false;
      checkState();
      return true;
    }});
    steps_.push_back({"wide navigation ready", [this] { return sized() && label("Documents"); }});
    addClick("visit document list", [this] { return label("Documents"); });
    steps_.push_back({"document list is mounted", [this] {
      auto* list = id("document-list");
      if (!list || visibleRect(list).isEmpty() || !list->property("contentY").isValid() ||
          list->property("contentHeight").toDouble() <= list->height()) return false;
      list_ = list;
      return true;
    }});
    addClick("select the first document", [this] { return availableLabel("Document 0001"); });
    steps_.push_back({"document selection updates React", [this] {
      auto* selected = availableLabel("Document 0001");
      if (!selected || !selected->property("accessibilityStateSelected").toBool() ||
          !text(QStringLiteral("Selected: document-0001"))) return false;
      capture("02-documents");
      return true;
    }});
    steps_.push_back({"scroll selected document list", [this] {
      require(list_, "Document list disappeared before wheel input");
      wheel(list_, -1);
      return true;
    }});
    steps_.push_back({"native wheel scroll", [this] {
      require(list_, "Document list was replaced during wheel input");
      if (list_->property("contentY").toDouble() <= 0 || list_->property("moving").toBool()) return false;
      listOffset_ = list_->property("contentY").toDouble();
      return true;
    }});
    addClick("visit sections", [this] { return label("Sections"); });
    steps_.push_back({"section list shares selection", [this] {
      auto* active = route("gallery-route-sections");
      if (!active || !fullyVisible(textItem(active, QStringLiteral("Layout and content"))) ||
          !fullyVisible(textItem(active, QStringLiteral("Selected: document-0001")))) return false;
      capture("03-sections");
      return true;
    }});
    addClick("visit controlled inputs", [this] { return label("Controls"); });
    steps_.push_back({"controls share edited state", [this] {
      auto* active = route("gallery-route-controls");
      if (!active || !fullyVisible(textItem(active, QStringLiteral("Observed text: ") + editedText_)) ||
          !fullyVisible(textItem(active, QStringLiteral("Switch ON")))) return false;
      return label("Press count: 1") != nullptr;
    }});
    addClick("press from the controls route", [this] { return availableLabel("Press count: 1"); });
    steps_.push_back({"controls update shared counter", [this] {
      if (!label("Press count: 2")) return false;
      expectedPresses_ = 2;
      checkState();
      capture("04-controls");
      return true;
    }});
    addClick("visit adaptive layout", [this] { return label("Adaptive"); });
    steps_.push_back({"adaptive route observes client and selection", [this] {
      auto* active = route("gallery-route-adaptive");
      if (!active || !observedClientSize(active) ||
          !fullyVisible(textItem(active, QStringLiteral("Selected document-0001"))) ||
          !fullyVisible(textItem(active, QStringLiteral("Layout details")))) return false;
      capture("05-adaptive");
      return true;
    }});
    addClick("visit limits", [this] { return label("Limits"); });
    steps_.push_back({"limits route is rendered", [this] {
      auto* active = route("gallery-route-limits");
      if (!active || !fullyVisible(textItem(active, QStringLiteral("Evidence and unsupported work")))) return false;
      capture("06-limits");
      return true;
    }});
    addClick("return to components", [this] { return label("Components"); });
    steps_.push_back({"gallery retains state after navigation", [this] {
      if (!route("gallery-route-gallery")) return false;
      checkState();
      return true;
    }});
    addClick("press after returning to components", [this] { return press_.data(); });
    steps_.push_back({"shared counter and controls remain interactive", [this] {
      if (!label("Press count: 3")) return false;
      expectedPresses_ = 3;
      checkState();
      capture("08-components-retained-state");
      return true;
    }});
    addClick("revisit documents", [this] { return label("Documents"); });
    steps_.push_back({"document list retains native scroll node", [this] {
      if (visibleRect(list_).isEmpty()) return false;
      require(list_ == id("document-list"), "Navigation remounted the list");
      require(std::abs(list_->property("contentY").toDouble() - listOffset_) < 1,
          "Navigation reset the document scroll offset");
      auto* selected = availableLabel("Document 0001");
      require(selected && selected->property("accessibilityStateSelected").toBool() &&
              text(QStringLiteral("Selected: document-0001")),
          "Navigation lost the selected document");
      return true;
    }});
    if (QCoreApplication::arguments().contains(QStringLiteral("--desktop"))) defineMinimumSteps();
  }

  void defineMinimumSteps() {
    addClick("visit adaptive before minimum resize", [this] { return label("Adaptive"); });
    steps_.push_back({"resize outer window to the desktop minimum", [this] {
      if (!route("gallery-route-adaptive")) return false;
      window_->resize(window_->minimumSize());
      return true;
    }});
    steps_.push_back({"React matches the actual minimum client", [this] {
      if (window_->size() != window_->minimumSize() || !followsClient()) return false;
      auto* active = route("gallery-route-adaptive");
      if (!active || !observedClientSize(active) || !label("Browse sections")) return false;
      require(active->height() >= 100, "Minimum window leaves no usable page viewport");
      require(viewport_->width() < window_->width() && viewport_->height() < window_->height(),
          "Desktop client incorrectly includes native chrome");
      std::cout << "RNQNX_GALLERY_MINIMUM window=" << window_->width() << 'x' << window_->height()
                << " client=" << viewport_->width() << 'x' << viewport_->height() << '\n';
      return true;
    }});
    steps_.push_back({"minimum adaptive heading and detail remain readable", [this] {
      auto* active = route("gallery-route-adaptive");
      auto* heading = textItem(active, QStringLiteral("Layout details"));
      auto* detail = textItem(active, QStringLiteral("Actual window observations."));
      if (!heading || !detail || !reveal(heading, detail)) return false;
      auto* scroll = heading->parentItem();
      while (scroll && !scroll->property("contentY").isValid()) scroll = scroll->parentItem();
      require(scroll && visibleRect(scroll).height() >= 100, "Minimum adaptive scroll viewport is unusable");
      capture("09-adaptive-minimum");
      return true;
    }});
    // Compact navigation is deliberately scrollable at the minimum height.
    // Reach every destination through native input instead of requiring all
    // six rows to fit on screen at once.
    for (const auto destination : kRoutes) {
      addClick("open minimum-window navigation", [this] { return label("Browse sections"); });
      addClick(destination.label, [this, destination] { return availableLabel(destination.label); });
      steps_.push_back({"minimum destination is reachable", [this, destination] {
        if (modalVisible()) return false;
        auto* active = route(destination.testId);
        if (!active) return false;
        require(active->height() >= 100, "Minimum destination has no usable page viewport");
        checkState();
        return true;
      }});
    }
    steps_.push_back({"restore wide desktop window", [this] {
      window_->resize(1100, 760);
      return true;
    }});
    steps_.push_back({"wide desktop client settles", [this] {
      return window_->size() == QSize(1100, 760) && followsClient() && label("Adaptive");
    }});
    addClick("wide adaptive layout after minimum", [this] { return label("Adaptive"); });
    steps_.push_back({"wide adaptive columns match actual client", [this] {
      auto* active = route("gallery-route-adaptive");
      auto* dimensions = findItem(active, [](auto* item) {
        return item->property("text").toString().startsWith(QStringLiteral("Window: "));
      });
      if (!reveal(dimensions) || !observedClientSize(active)) return false;
      auto* document = id("example-card-Document");
      auto* details = id("example-card-Layout details");
      if (!presented(document) || !presented(details)) return false;
      const auto first = document->mapRectToItem(active, document->boundingRect());
      const auto second = details->mapRectToItem(active, details->boundingRect());
      if (std::abs(first.top() - second.top()) > 0.6 || second.left() <= first.right()) return false;
      require(first.left() >= -0.6 && second.right() <= active->width() + 0.6,
          "Wide adaptive cards overflowed the real client region");
      return true;
    }});
    addClick("return from minimum qualification", [this] { return label("Components"); });
    steps_.push_back({"desktop retained state and final capture", [this] {
      if (!route("gallery-route-gallery")) return false;
      checkState();
      require(checkGrid() >= 2, "Restoring the wide desktop left a narrow gallery");
      capture("10-desktop-gallery");
      return true;
    }});
  }

  void advance() {
    if (advancing_) return;
    QScopedValueRollback<bool> guard(advancing_, true);
    try {
      const auto& step = steps_.at(index_);
      if (phaseTime_.elapsed() > 8000) throw std::runtime_error(std::string("Timed out: ") + step.name);
      if (step.action()) {
        std::cout << "RNQNX_GALLERY_STEP " << index_ << ' ' << step.name
                  << " elapsed_ms=" << inputClock_.elapsed() << std::endl;
        ++index_;
        phaseTime_.restart();
        if (index_ == steps_.size()) {
          timer_.stop();
          std::cout << "RNQNX_GALLERY_INTERACTIONS_COMPLETE routes=6 presses=3 switch=on"
                       " selected=document-0001 modal=button-and-Escape native-layout resize state caret focus wheel scroll"
                    << " lifecycle=" << RNQNX_ENABLE_INPUT_LIFECYCLE
                    << " client=" << viewport_->width() << 'x' << viewport_->height() << std::endl;
          QCoreApplication::exit(0);
        }
      }
    } catch (const std::exception& error) {
      timer_.stop();
      std::cerr << "RNQNX_GALLERY_SMOKE_FAIL step=" << steps_.at(index_).name << " " << error.what() << std::endl;
      try {
        if (window_) capture("failure");
      } catch (const std::exception& captureError) {
        std::cerr << "Screenshot after failure unavailable: " << captureError.what() << '\n';
      }
      QCoreApplication::exit(1);
    }
  }

  QTimer timer_;
  QElapsedTimer phaseTime_;
  QElapsedTimer held_;
  QElapsedTimer inputClock_;
  QElapsedTimer lastWheel_;
  std::vector<Step> steps_;
  std::size_t index_ = 0;
  bool advancing_ = false;
  QPointer<QQuickWindow> window_;
  QPointer<QQuickItem> viewport_, press_, field_, editor_, list_;
  QSizeF requestedSize_;
  QPointF point_;
  QString editedText_;
  int caret_ = 0;
  int expectedPresses_ = 1;
  double listOffset_ = 0;
};

void startGalleryDriver() {
  QTimer::singleShot(0, [] { new GalleryDriver(); });
}
Q_COREAPP_STARTUP_FUNCTION(startGalleryDriver)
}  // namespace

int main(int argc, char** argv) {
  return rnqnxPlaygroundMain(argc, argv);
}
