#include "WindowChrome.h"

#include "WindowChromeQml.generated.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rnqnx::window {

WindowChrome::WindowChrome(QQuickWindow& window, QQmlEngine& engine, WindowChromeConfig config,
                           WindowCapabilities capabilities)
    : window_(&window), config_(std::move(config)), capabilities_(std::move(capabilities)) {
  layout_ = layoutChrome(config_, capabilities_, window.width());
  for (const auto& slot : layout_.controls) {
    controls_.append(QVariantMap{{QStringLiteral("action"), nameOf(slot.action)},
                                 {QStringLiteral("name"), slot.accessibleName},
                                 {QStringLiteral("enabled"), slot.enabled}});
  }

  content_ = std::make_unique<QQuickItem>();
  content_->setObjectName(QStringLiteral("rnqnxContentRoot"));
  content_->setParentItem(window.contentItem());
  content_->setClip(true);

  QQmlComponent component(&engine);
  component.setData(QByteArray(kWindowChromeQml),
                    QUrl(QStringLiteral("qrc:/rnqnx/WindowChrome.qml")));
  QObject* created = component.createWithInitialProperties(
      {{QStringLiteral("chrome"), QVariant::fromValue(static_cast<QObject*>(this))}});
  auto* item = qobject_cast<QQuickItem*>(created);
  if (item == nullptr) {
    delete created;
    throw std::runtime_error(("Window chrome QML failed to load: " + component.errorString())
                                 .toStdString());
  }
  chrome_.reset(item);
  chrome_->setParentItem(window.contentItem());

  connect(&window, &QWindow::widthChanged, this, &WindowChrome::relayout);
  connect(&window, &QWindow::heightChanged, this, &WindowChrome::relayout);
  connect(&window, &QWindow::windowTitleChanged, this, &WindowChrome::titleChanged);
  connect(&window, &QWindow::activeChanged, this, &WindowChrome::stateChanged);
  connect(&window, &QWindow::windowStateChanged, this, &WindowChrome::stateChanged);
  relayout();
}

WindowChrome::~WindowChrome() {
  // Items go before the window's content item; Fabric unmounts children of
  // content_ before this object is destroyed (it outlives the host in main).
  chrome_.reset();
  content_.reset();
}

QQuickItem& WindowChrome::contentRoot() const { return *content_; }

QQuickItem* WindowChrome::chromeItem() const { return chrome_.get(); }

QVariantMap WindowChrome::controlGeometry() const {
  QVariantMap result;
  for (const auto& slot : layout_.controls) {
    result.insert(nameOf(slot.action), QVariantMap{{QStringLiteral("x"), slot.hitRect.x()},
                                                   {QStringLiteral("y"), slot.hitRect.y()},
                                                   {QStringLiteral("width"), slot.hitRect.width()},
                                                   {QStringLiteral("height"), slot.hitRect.height()}});
  }
  return result;
}

qreal WindowChrome::windowWidth() const { return window_ ? window_->width() : 0; }

QString WindowChrome::title() const { return window_ ? window_->title() : QString(); }

bool WindowChrome::active() const { return window_ && window_->isActive(); }

bool WindowChrome::maximized() const {
  return window_ && (window_->windowStates() & Qt::WindowMaximized);
}

QStringList WindowChrome::describeHiddenControls() const {
  QStringList lines;
  for (const auto& hidden : layout_.hidden) {
    lines.append(QStringLiteral("RNQNX_WINDOW_CHROME platform=%1 hidden=%2 reason=%3")
                     .arg(capabilities_.platform, nameOf(hidden.action), hidden.reason));
  }
  for (const auto& disabled : layout_.disabled) {
    lines.append(QStringLiteral("RNQNX_WINDOW_CHROME platform=%1 disabled=%2 reason=%3")
                     .arg(capabilities_.platform, nameOf(disabled.action), disabled.reason));
  }
  return lines;
}

void WindowChrome::trigger(const QString& action) {
  if (!window_) return;
  // Only controls that are laid out can act; a stale QML call for a hidden
  // control does nothing.
  const ControlSlot* found = nullptr;
  for (const auto& slot : layout_.controls)
    if (nameOf(slot.action) == action) found = &slot;
  if (found == nullptr) return;
  if (!found->enabled) {
    Q_EMIT actionUnavailable(action, found->reason);
    return;
  }
  Q_EMIT actionTriggered(action);
  if (action == QLatin1String("close")) {
    window_->close();
  } else if (action == QLatin1String("minimize")) {
    window_->showMinimized();
  } else if (action == QLatin1String("maximize")) {
    if (maximized())
      window_->showNormal();
    else
      window_->showMaximized();
  }
}

void WindowChrome::setIconSource(const QUrl& source) {
  if (source == iconSource_) return;
  iconSource_ = source;
  Q_EMIT iconChanged();
}

void WindowChrome::beginSystemMove() {
  if (window_ && capabilities_.systemMove) window_->startSystemMove();
}

void WindowChrome::relayout() {
  if (!window_) return;
  layout_ = layoutChrome(config_, capabilities_, window_->width());
  content_->setPosition(QPointF(0, layout_.contentInset));
  content_->setSize(QSizeF(window_->width(), std::max<qreal>(0, window_->height() - layout_.contentInset)));
  Q_EMIT layoutChanged();
}

}  // namespace rnqnx::window
