#include "WindowSession.h"
#include "WindowHost.h"
#include <QCloseEvent>
#include <QQuickItem>
#include <QEvent>
#include <QThread>
#include <QUuid>
#include <limits>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rnqnx::window {
bool WindowSessionState::submit(std::function<void(WindowSession&)> run,
                                std::function<void()> reject) {
  std::lock_guard lock(mutex);
  if (!owner) return false;
  const auto id = nextId++;
  pending.emplace(id, Pending{std::move(run), std::move(reject)});
  const bool posted = QMetaObject::invokeMethod(owner, [this, id] {
    Pending operation;
    WindowSession* session;
    {
      std::lock_guard admission(mutex);
      auto found = pending.find(id);
      if (found == pending.end()) return;
      operation = std::move(found->second);
      pending.erase(found);
      session = owner;
    }
    if (session) operation.run(*session);
    else operation.reject();
  }, Qt::QueuedConnection);
  if (!posted) pending.erase(id);
  return posted;
}

std::uint64_t WindowSessionState::listen(Change change) {
  std::uint64_t id, version;
  QVariantMap initial;
  {
    std::lock_guard lock(mutex);
    if (!owner) throw std::runtime_error("The native window is closed");
    id = nextId++;
    version = sequence;
    initial = snapshot;
    listeners.emplace(id, change);
  }
  try { change(version, initial, true); }
  catch (...) { unlisten(id); throw; }
  return id;
}
void WindowSessionState::unlisten(std::uint64_t id) {
  std::lock_guard lock(mutex);
  listeners.erase(id);
}
std::shared_ptr<std::atomic<bool>> WindowSessionState::guard() {
  auto active = std::make_shared<std::atomic<bool>>(true);
  std::lock_guard lock(mutex);
  if (!owner) throw std::runtime_error("The native window is closed");
  if (!closeVetoAvailable) throw std::runtime_error("The window manager cannot preserve a close veto");
  guards.erase(std::remove_if(guards.begin(), guards.end(), [](const auto& weak) {
    const auto guard = weak.lock(); return !guard || !guard->load();
  }), guards.end());
  guards.emplace_back(active);
  return active;
}
bool WindowSessionState::blocked() {
  std::lock_guard lock(mutex);
  bool blocked = false;
  for (auto it = guards.begin(); it != guards.end();) {
    if (auto active = it->lock()) { blocked = blocked || active->load(); ++it; }
    else it = guards.erase(it);
  }
  return blocked;
}

WindowSession::WindowSession(QQuickWindow& window, WindowChrome* chrome,
                             WindowCapabilities capabilities)
    : window_(&window), chrome_(chrome), capabilities_(std::move(capabilities)),
      state_(std::make_shared<WindowSessionState>()) {
  Q_ASSERT(QThread::currentThread() == window.thread());
  state_->owner = this;
  closeRequestEpoch_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  state_->closeVetoAvailable = capabilities_.platform != QLatin1String("qnx") ||
                                windowManagerSupportsCloseVeto(window);
  const auto platform = capabilities_.platform;
  canPosition_ = !capabilities_.containedWindow &&
      (capabilities_.systemMove || capabilities_.clientMoveResize || platform == QLatin1String("offscreen"));
  canResize_ = !capabilities_.containedWindow && !platform.startsWith(QLatin1String("wayland"));
  window.installEventFilter(this);
  connect(&window, &QWindow::widthChanged, this, [this] { publish(); });
  connect(&window, &QWindow::heightChanged, this, [this] { publish(); });
  connect(&window, &QWindow::xChanged, this, [this] { publish(); });
  connect(&window, &QWindow::yChanged, this, [this] { publish(); });
  connect(&window, &QWindow::windowTitleChanged, this, [this] { publish(); });
  connect(&window, &QWindow::activeChanged, this, [this] { publish(); });
  connect(&window, &QWindow::visibilityChanged, this, [this] {
    if (window_ && window_->isVisible()) minimized_ = false;
    publish();
  });
  connect(&window, &QWindow::windowStateChanged, this, [this] { publish(); });
  connect(&window, &QObject::destroyed, this, [this] { window_ = nullptr; shutdown(); });
  if (chrome) {
    connect(chrome, &WindowChrome::layoutChanged, this, [this] { publish(); });
    connect(chrome, &WindowChrome::actionTriggered, this, [this](const QString& action) {
      if (action == QLatin1String("minimize")) minimized_ = true;
    });
  }
  publish();
}
WindowSession::~WindowSession() { shutdown(); }
void WindowSession::requireWindow() const {
  Q_ASSERT(thread() == QThread::currentThread());
  if (!window_) throw std::runtime_error("The native window is closed");
}
void WindowSession::shutdown() { state_->shutdown(); }
void WindowSessionState::shutdown() {
  std::map<std::uint64_t, WindowSessionState::Pending> pending;
  std::map<std::uint64_t, WindowSessionState::Change> listeners;
  {
    std::lock_guard lock(mutex);
    owner = nullptr;
    pending.swap(this->pending);
    listeners.swap(this->listeners);
    for (const auto& weak : guards) if (auto active = weak.lock()) active->store(false);
    guards.clear();
  }
  for (auto& [id, operation] : pending) { (void)id; operation.reject(); }
}
void WindowSession::publish() {
  if (!window_) return;
  QVariantMap insets{{QStringLiteral("top"), 0.0}, {QStringLiteral("right"), 0.0},
                    {QStringLiteral("bottom"), 0.0}, {QStringLiteral("left"), 0.0}};
  if (chrome_) insets = chrome_->titleBarInset();
  QVariantMap next{{QStringLiteral("title"), window_->title()},
    {QStringLiteral("x"), window_->x()}, {QStringLiteral("y"), window_->y()},
    {QStringLiteral("width"), window_->width()}, {QStringLiteral("height"), window_->height()},
    {QStringLiteral("contentWidth"), chrome_ ? chrome_->contentRoot().width() : window_->width()},
    {QStringLiteral("contentHeight"), chrome_ ? chrome_->contentRoot().height() : window_->height()},
    {QStringLiteral("visible"), window_->isVisible()}, {QStringLiteral("focused"), window_->isActive()},
    {QStringLiteral("minimized"), minimized_ || window_->windowState() == Qt::WindowMinimized},
    {QStringLiteral("closeRequested"), closeRequested_},
    {QStringLiteral("closeRequestId"), closeRequestId_},
    {QStringLiteral("controlsLayout"), chrome_ ? chrome_->controlsLayout() : QStringLiteral("none")},
    {QStringLiteral("insets"), insets}, {QStringLiteral("canSetPosition"), canPosition_},
    {QStringLiteral("canResize"), canResize_},
    {QStringLiteral("canPreventClose"), state_->closeVetoAvailable}, {QStringLiteral("canMinimize"), capabilities_.minimize.available}};
  std::vector<WindowSessionState::Change> listeners;
  std::uint64_t version;
  {
    std::lock_guard lock(state_->mutex);
    if (!state_->owner) return;
    state_->snapshot = next;
    version = ++state_->sequence;
    for (const auto& [id, listener] : state_->listeners) { (void)id; listeners.push_back(listener); }
  }
  for (const auto& listener : listeners) listener(version, next, false);
}
void WindowSession::setTitle(const QString& title) {
  requireWindow();
  if (title.size() > 4096 || title.contains(QChar(0))) throw std::invalid_argument("Invalid window title");
  window_->setTitle(title);
}
void WindowSession::setSize(double width, double height) {
  requireWindow();
  if (!canResize_) throw std::runtime_error("Window resizing is unsupported by this target");
  if (!std::isfinite(width) || !std::isfinite(height) || width != std::floor(width) ||
      height != std::floor(height) || width < 1 || height < 1 || width > 16384 || height > 16384)
    throw std::invalid_argument("Window dimensions must be integers from 1 to 16384");
  window_->resize(static_cast<int>(width), static_cast<int>(height));
}
void WindowSession::setPosition(double x, double y) {
  requireWindow();
  if (!canPosition_) throw std::runtime_error("Window positioning is unsupported by this target");
  if (!std::isfinite(x) || !std::isfinite(y) || x != std::floor(x) || y != std::floor(y) ||
      x < -16384 || x > 16384 || y < -16384 || y > 16384)
    throw std::invalid_argument("Window position must be integer logical pixels from -16384 to 16384");
  window_->setPosition(static_cast<int>(x), static_cast<int>(y));
  applyNativePosition(*window_);
}
void WindowSession::setControlsLayout(const QString& layout) {
  requireWindow();
  if (!chrome_) throw std::runtime_error("Window chrome is not enabled");
  if (!chrome_->setControlsLayout(layout)) throw std::invalid_argument("Invalid window controls layout");
}
void WindowSession::minimize() {
  requireWindow();
  if (!capabilities_.minimize.available) throw std::runtime_error("Minimize is unsupported by this target");
  minimized_ = true;
  if (capabilities_.platform == QLatin1String("qnx")) {
    if (capabilities_.clientMoveResize) sendWindowManagerRequest(*window_, "minimize");
    else minimizeByDestroyingWindow(*window_);
  } else window_->showMinimized();
  publish();
}
void WindowSession::restore() {
  requireWindow();
  if (capabilities_.platform == QLatin1String("qnx"))
    restoreMinimizedWindow(*window_, capabilities_.clientMoveResize);
  else window_->showNormal();
  minimized_ = false;
  publish();
}
void WindowSession::setVisible(bool visible) { requireWindow(); window_->setVisible(visible); }
void WindowSession::requestFocus() { requireWindow(); window_->requestActivate(); }
bool WindowSession::close() { requireWindow(); return window_->close(); }
bool WindowSession::cancelCloseRequest(const QString& requestId) {
  requireWindow();
  if (!closeRequested_ || requestId != closeRequestId_) return false;
  closeRequested_ = false;
  publish();
  return true;
}
bool WindowSession::eventFilter(QObject* object, QEvent* event) {
  if (object == window_ && event->type() == QEvent::Close && state_->blocked()) {
    closeRequested_ = true;
    if (closeRequestSequence_ == std::numeric_limits<std::uint64_t>::max()) {
      closeRequestEpoch_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
      closeRequestSequence_ = 0;
    }
    closeRequestId_ = closeRequestEpoch_ + QLatin1Char(':') + QString::number(++closeRequestSequence_);
    if (capabilities_.platform == QLatin1String("qnx") && !acknowledgeWindowCloseVeto(*window_))
      qWarning("RNQNX_WINDOW_CLOSE_VETO_FAILED: manager acknowledgement was refused");
    static_cast<QCloseEvent*>(event)->ignore();
    publish();
    return true;
  }
  if (object == window_ && event->type() == QEvent::Close) {
    closeRequested_ = false;
    publish();
  }
  return QObject::eventFilter(object, event);
}
} // namespace rnqnx::window
