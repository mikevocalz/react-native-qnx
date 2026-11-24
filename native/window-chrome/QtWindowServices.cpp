#include "QtWindowServices.h"

#include "HostWindowState.h"
#include "WindowChrome.h"
#include "WindowHost.h"

#include <QQuickWindow>
#include <QScreen>
#include <QThread>

#include "react-native-qnx/runtime/QueuedCallTarget.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rnqnx::window {

namespace {
bool movesNativeWindow(const WindowCapabilities& capabilities, bool forcedFullScreen) {
  if (capabilities.containedWindow || forcedFullScreen) return false;
  // QNX with the stock manager keeps every window at the display size; only
  // rnqnx-winmgr (clientMoveResize) lets the app place its window.
  return capabilities.clientMoveResize || capabilities.platform != QLatin1String("qnx");
}
}  // namespace

std::vector<std::string> QtWindowServices::capabilityNames(const WindowCapabilities& capabilities,
                                                           bool forcedFullScreen) {
  std::vector<std::string> names{"close"};
  if (capabilities.minimize.available) names.emplace_back("minimize");
  if (capabilities.maximize.available) names.emplace_back("maximize");
  if (movesNativeWindow(capabilities, forcedFullScreen)) {
    names.emplace_back("move");
    names.emplace_back("resize");
  }
  return names;
}

QtWindowServices::QtWindowServices(QQuickWindow& window, WindowChrome& chrome,
                                   WindowCloseGate& gate, WindowCapabilities capabilities,
                                   bool forcedFullScreen, runtime::QueuedCallTarget& gui,
                                   QObject* parent)
    : QObject(parent),
      window_(window),
      chrome_(chrome),
      gate_(gate),
      capabilities_(std::move(capabilities)),
      gui_(gui) {
  auto& s = *services_;
  s.capabilities = capabilityNames(capabilities_, forcedFullScreen);
  s.state = publisher_;
  s.epoch = gate_.epochCounter();

  // Requests run on the GUI thread. Each `done` runs there too.
  s.setTitle = [this](std::string title, std::function<void(std::string)> done) {
    onGui([this, title = std::move(title), done = std::move(done)] {
      window_.setTitle(QString::fromStdString(title));
      refresh();
      done({});
    });
  };
  s.setControlsLayout = [this](std::string layout, std::function<void(std::string)> done) {
    onGui([this, layout = std::move(layout), done = std::move(done)] {
      const bool ok = chrome_.setControlsLayout(QString::fromStdString(layout));
      refresh();
      done(ok ? std::string() : "INVALID_ARGUMENT: unknown controls layout");
    });
  };
  s.setBounds = [this](nitro::Rect bounds, std::function<void(std::string, nitro::Rect)> done) {
    onGui([this, bounds, done = std::move(done)] { setBounds(bounds, done); });
  };
  s.minimize = [this](std::function<void(std::string)> done) {
    onGui([this, done = std::move(done)] { minimize(done); });
  };
  s.requestClose = [this](bool force, std::function<void(bool)> done) {
    onGui([this, force, done = std::move(done)] { gate_.requestClose(force, done); });
  };
  s.guardAdded = [this](std::uint64_t id) { onGui([this, id] { gate_.addGuard(id); }); };
  s.guardRemoved = [this](std::uint64_t id) { onGui([this, id] { gate_.removeGuard(id); }); };
  s.settleClose = [this](std::uint64_t requestId, std::uint64_t epoch, bool allow) {
    onGui([this, requestId, epoch, allow] { gate_.settle(requestId, epoch, allow); });
  };
  s.runtimeGone = [this] {
    onGui([this] {
      handlers_ = {};
      gate_.setRequestHandler({});
      gate_.runtimeGone();
    });
  };
  s.connect = [this](nitro::WindowEventHandlers handlers) {
    onGui([this, handlers = std::move(handlers)]() mutable {
      handlers_ = std::move(handlers);
      auto onClose = handlers_.onCloseRequest;
      gate_.setRequestHandler(
          [onClose](const CloseRequestInfo& info) { if (onClose) onClose(info); });
    });
  };

  // Publish on every change that can alter the snapshot.
  const auto again = [this] { refresh(); };
  QObject::connect(&window_, &QWindow::xChanged, this, again);
  QObject::connect(&window_, &QWindow::yChanged, this, again);
  QObject::connect(&window_, &QWindow::widthChanged, this, again);
  QObject::connect(&window_, &QWindow::heightChanged, this, again);
  QObject::connect(&window_, &QWindow::windowTitleChanged, this, again);
  QObject::connect(&window_, &QWindow::activeChanged, this, again);
  QObject::connect(&window_, &QWindow::visibleChanged, this, again);
  QObject::connect(&window_, &QWindow::visibilityChanged, this, again);
  QObject::connect(&window_, &QWindow::windowStateChanged, this, again);
  QObject::connect(&chrome_, &WindowChrome::stateChanged, this, again);
  QObject::connect(&chrome_, &WindowChrome::controlsLayoutChanged, this, again);
  QObject::connect(&chrome_, &WindowChrome::layoutChanged, this, [this] { refreshInset(); });
  QObject::connect(&runtime::HostWindowState::instance(),
                   &runtime::HostWindowState::minimizedChanged, this, again);
  // The close control announces itself, so a close nobody announced is the platform's.
  QObject::connect(&chrome_, &WindowChrome::actionTriggered, this, [this](const QString& action) {
    if (action == QLatin1String("close")) gate_.noteNextClose(CloseReason::Control, true, -1);
  });
  // A close the platform sends is `system`. Under rnqnx-winmgr (and the stock
  // QNX manager) the process ends after the grace period whatever a guard says,
  // so the guard may save but not cancel. Elsewhere a native frame close can be vetoed.
  gate_.setSystemClose(capabilities_.platform != QLatin1String("qnx"), -1);
  gate_.setVisibleProbe([] { return !runtime::HostWindowState::instance().minimized(); });
  const WindowState first = snapshot();
  lastVisible_ = first.visible;
  lastFocused_ = first.focused;
  publisher_->publish(first);
  refreshInset();
}

QtWindowServices::~QtWindowServices() = default;

void QtWindowServices::onGui(std::function<void()> work) {
  if (QThread::currentThread() == gui_.thread()) {
    work();
  } else {
    gui_.postCall(std::move(work));
  }
}

WindowState QtWindowServices::snapshot() const {
  WindowState state;
  state.title = window_.title().toStdString();
  const QRect geometry = window_.geometry();
  if (chrome_.contained()) {
    const QRectF frame = chrome_.frameRect();
    state.x = geometry.x() + frame.x();
    state.y = geometry.y() + frame.y();
    state.width = frame.width();
    state.height = frame.height();
  } else {
    state.x = geometry.x();
    state.y = geometry.y();
    state.width = geometry.width();
    state.height = geometry.height();
  }
  state.visible = window_.isVisible() && window_.visibility() != QWindow::Minimized &&
                  window_.visibility() != QWindow::Hidden &&
                  !runtime::HostWindowState::instance().minimized();
  // A hidden window has no focus: minimize flips both, visibility first.
  state.focused = state.visible && window_.isActive();
  state.maximized = chrome_.maximized();
  state.controlsLayout = chrome_.controlsLayout().toStdString();
  return state;
}

void QtWindowServices::refresh() {
  const WindowState next = snapshot();
  const auto published = publisher_->publish(next);
  if (!published) return;
  // Order for one change: snapshot, then visibility, then focus. Focus and
  // visibility posts are never coalesced.
  if (published->visible != lastVisible_) {
    lastVisible_ = published->visible;
    if (handlers_.onVisibilityChanged) handlers_.onVisibilityChanged(published->visible);
  }
  if (published->focused != lastFocused_) {
    lastFocused_ = published->focused;
    if (handlers_.onFocusChanged) handlers_.onFocusChanged(published->focused);
  }
}

void QtWindowServices::refreshInset() {
  const TitleBarInset& inset = chrome_.layout().inset;
  WindowInset next;
  next.top = inset.top;
  next.right = inset.right;
  next.bottom = inset.bottom;
  next.left = inset.left;
  next.overlayX = inset.overlay.x();
  next.overlayY = inset.overlay.y();
  next.overlayWidth = inset.overlay.width();
  next.overlayHeight = inset.overlay.height();
  if (publisher_->publishInset(next) && handlers_.onInsetChanged) handlers_.onInsetChanged();
}

void QtWindowServices::setBounds(nitro::Rect bounds,
                                 std::function<void(std::string, nitro::Rect)> done) {
  QRect available = window_.screen() != nullptr ? window_.screen()->availableGeometry()
                                                : QRect(0, 0, 16384, 16384);
  const int width = std::clamp(static_cast<int>(std::lround(bounds.width)),
                               std::min(kMinWindowWidth, available.width()), available.width());
  const int height = std::clamp(static_cast<int>(std::lround(bounds.height)),
                                std::min(kMinWindowHeight, available.height()), available.height());
  const int x = std::clamp(static_cast<int>(std::lround(bounds.x)), available.left(),
                           available.right() - width + 1);
  const int y = std::clamp(static_cast<int>(std::lround(bounds.y)), available.top(),
                           available.bottom() - height + 1);
  window_.setGeometry(QRect(x, y, width, height));
  applyNativePosition(window_);
  refresh();
  const QRect applied = window_.geometry();
  done({}, nitro::Rect{static_cast<double>(applied.x()), static_cast<double>(applied.y()),
                       static_cast<double>(applied.width()), static_cast<double>(applied.height())});
}

void QtWindowServices::minimize(std::function<void(std::string)> done) {
  if (!capabilities_.minimize.available) {
    done("WINDOW_ACTION_UNSUPPORTED: " + capabilities_.minimize.reason.toStdString());
    return;
  }
  // The same paths as the control, without needing the control to be drawn.
  if (capabilities_.clientMoveResize) {
    sendWindowManagerRequest(window_, "minimize");
  } else if (capabilities_.platform == QLatin1String("qnx")) {
    minimizeByDestroyingWindow(window_);
  } else {
    window_.showMinimized();
  }
  refresh();
  done({});
}

}  // namespace rnqnx::window
