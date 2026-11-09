#include "react-native-qnx/views/QtDesktopWindow.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QResource>
#include <QVariant>

#include <stdexcept>

static void initializeDesktopResources() {
  Q_INIT_RESOURCE(rnqnx_desktop_window);
}

namespace rnqnx::views {

QtDesktopWindow::QtDesktopWindow(QQmlEngine& engine, QQuickWindow& window,
    WindowFrame frame) {
  initializeDesktopResources();
  QQmlComponent component(&engine,
      QUrl(QStringLiteral("qrc:/rnqnx/desktop/DesktopWindow.qml")));
  const QString platform = QGuiApplication::platformName();
  // The QNX QPA minimize() implementation reports unsupported. Do not hide a
  // window without a qualified desktop provider that can restore it.
  const bool canMinimize = platform == QStringLiteral("xcb") ||
      platform == QStringLiteral("wayland") || platform == QStringLiteral("cocoa") ||
      platform == QStringLiteral("windows") || platform == QStringLiteral("offscreen");
  std::unique_ptr<QObject> object(component.createWithInitialProperties({
      {QStringLiteral("appWindow"), QVariant::fromValue(&window)},
      {QStringLiteral("clientSideFrame"), frame == WindowFrame::Client},
      {QStringLiteral("nativeMinimizeAvailable"), canMinimize}}));
  auto* item = qobject_cast<QQuickItem*>(object.get());
  if (item == nullptr) {
    throw std::runtime_error(component.errorString().toStdString());
  }
  content_ = item->findChild<QQuickItem*>(QStringLiteral("rnqnxDesktopContent"));
  if (content_ == nullptr) {
    throw std::runtime_error("Desktop window has no React content viewport");
  }
  root_.reset(item);
  object.release();
  root_->setParentItem(window.contentItem());
  if (frame == WindowFrame::Client) {
    window.setFlags(window.flags() | Qt::FramelessWindowHint);
  }
  window.setMinimumSize(QSize(480, 360));
}

QtDesktopWindow::~QtDesktopWindow() = default;

QQuickItem& QtDesktopWindow::contentItem() {
  return *content_;
}

}  // namespace rnqnx::views
