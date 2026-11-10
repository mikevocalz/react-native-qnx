#include "ExpoQnxApplication.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QThread>
#include <stdexcept>
namespace rnqnx::expo {
namespace {
std::optional<std::string> optionalString(const QString& value) {
  if (value.isEmpty()) return std::nullopt;
  return value.toUtf8().toStdString();
}
} // namespace
ApplicationMetadata readApplicationMetadata() {
  auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
  if (application == nullptr || application->thread() != QThread::currentThread()) {
    throw std::runtime_error("ExpoApplication requires the QGuiApplication owner thread");
  }
  return {optionalString(QGuiApplication::desktopFileName()),
          optionalString(QGuiApplication::applicationDisplayName()),
          optionalString(QCoreApplication::applicationVersion())};
}
} // namespace rnqnx::expo
