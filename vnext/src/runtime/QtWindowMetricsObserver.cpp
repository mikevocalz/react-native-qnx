#include "react-native-qnx/runtime/QtWindowMetricsObserver.h"

#include <QEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QThread>
#include <QWindow>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {
namespace {
bool positiveSize(const QSizeF& size) {
  return std::isfinite(size.width()) && std::isfinite(size.height()) &&
      size.width() > 0 && size.height() > 0;
}
bool usableScale(double value) {
  const float layoutValue = static_cast<float>(value);
  return std::isfinite(value) && std::isfinite(layoutValue) && layoutValue > 0;
}
}  // namespace

QtWindowMetricsObserver::QtWindowMetricsObserver(
    QWindow& window, QQuickItem& client, Changed changed)
    : window_(&window), client_(&client), changed_(std::move(changed)) {
  if (QThread::currentThread() != window.thread() ||
      QThread::currentThread() != client.thread() || client.window() != &window) {
    throw std::invalid_argument("Window metrics require this window's client on its Qt owner");
  }
  current_ = read();
  if (!positiveSize(current_.windowSize) || !usableScale(current_.windowScale) ||
      !usableScale(current_.screenScale)) {
    throw std::invalid_argument("Window metrics require positive initial client bounds and finite pixel ratios");
  }
  pending_.setSingleShot(true);
  QObject::connect(&pending_, &QTimer::timeout, this, [this] { refresh(); });
  QObject::connect(&client, &QQuickItem::widthChanged, this, [this] { scheduleRefresh(); });
  QObject::connect(&client, &QQuickItem::heightChanged, this, [this] { scheduleRefresh(); });
  QObject::connect(&client, &QObject::destroyed, this, [this] { close(); });
  QObject::connect(&window, &QObject::destroyed, this, [this] { close(); });
  QObject::connect(&window, &QWindow::screenChanged, this, [this](QScreen* screen) {
    watchScreen(screen);
    scheduleRefresh();
  });
  watchScreen(window.screen());
  window.installEventFilter(this);
}

QtWindowMetricsObserver::~QtWindowMetricsObserver() { close(); }

const WindowMetrics& QtWindowMetricsObserver::current() const noexcept { return current_; }

void QtWindowMetricsObserver::setFontScale(double scale) {
  if (QThread::currentThread() != thread() || !active_) {
    throw std::logic_error("Window metrics observation is closed or called off its owner");
  }
  if (!usableScale(scale)) {
    throw std::invalid_argument("Font scale must be finite and positive in the layout coordinate range");
  }
  if (fontScale_ == scale) return;
  fontScale_ = scale;
  // This is a synchronous host observation, not a preference-setting request.
  refresh();
}

void QtWindowMetricsObserver::close() noexcept {
  if (!active_) return;
  if (QThread::currentThread() != thread()) std::terminate();
  active_ = false;
  pending_.stop();
  if (window_) window_->removeEventFilter(this);
  for (const auto& connection : screenConnections_) QObject::disconnect(connection);
  screenConnections_.clear();
  if (window_) QObject::disconnect(window_, nullptr, this, nullptr);
  if (client_) QObject::disconnect(client_, nullptr, this, nullptr);
  changed_ = nullptr;
}

bool QtWindowMetricsObserver::eventFilter(QObject* watched, QEvent* event) {
  if (watched == window_ && event->type() == QEvent::DevicePixelRatioChange) scheduleRefresh();
  return false;
}

WindowMetrics QtWindowMetricsObserver::read() const {
  WindowMetrics result;
  result.fontScale = fontScale_;
  if (client_) result.windowSize = client_->size();
  if (window_) {
    result.windowScale = window_->devicePixelRatio();
    if (const auto* screen = window_->screen()) {
      result.screenSize = screen->geometry().size();
      result.screenScale = screen->devicePixelRatio();
    }
  }
  return result;
}

void QtWindowMetricsObserver::watchScreen(QScreen* screen) {
  for (const auto& connection : screenConnections_) QObject::disconnect(connection);
  screenConnections_.clear();
  if (!screen) return;
  screenConnections_.push_back(QObject::connect(screen, &QScreen::geometryChanged,
      this, [this] { scheduleRefresh(); }));
  screenConnections_.push_back(QObject::connect(screen, &QScreen::logicalDotsPerInchChanged,
      this, [this] { scheduleRefresh(); }));
  screenConnections_.push_back(QObject::connect(screen, &QScreen::physicalDotsPerInchChanged,
      this, [this] { scheduleRefresh(); }));
  screenConnections_.push_back(QObject::connect(screen, &QObject::destroyed,
      this, [this] { scheduleRefresh(); }));
}

void QtWindowMetricsObserver::scheduleRefresh() {
  if (active_ && !pending_.isActive()) pending_.start(0);
}

void QtWindowMetricsObserver::refresh() {
  if (!active_ || !window_ || !client_) return;
  const auto next = read();
  // Minimize/hide can briefly provide zero client geometry. Preserve the last
  // usable constraints until a real positive resize arrives; no guessed size.
  if (!positiveSize(next.windowSize) || !usableScale(next.windowScale) ||
      !usableScale(next.screenScale) ||
      next == current_) return;
  current_ = next;
  if (changed_) changed_(current_);
}

}  // namespace rnqnx::runtime
