#pragma once

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <stdexcept>

namespace rnqnx::runtime {

// Process metadata and React registration, not an authorization credential.
struct ApplicationIdentity {
  QString appId;
  QString displayName;
  QString componentName{QStringLiteral("RNQnxPlayground")};
  bool custom{false};
};

inline void validateApplicationIdentity(const ApplicationIdentity& identity) {
  static const QRegularExpression idPattern(
      QStringLiteral("\\A[A-Za-z][A-Za-z0-9_]*(?:\\.[A-Za-z][A-Za-z0-9_]*)+\\z"));
  static const QRegularExpression componentPattern(
      QStringLiteral("\\A[A-Za-z][A-Za-z0-9_]{0,127}\\z"));
  if (identity.appId.size() > 255 || !idPattern.match(identity.appId).hasMatch() ||
      identity.appId.endsWith(QStringLiteral(".desktop"))) {
    throw std::invalid_argument("App ID must be a reverse-DNS identifier without a .desktop suffix.");
  }
  if (!componentPattern.match(identity.componentName).hasMatch()) {
    throw std::invalid_argument("Component name must be an ASCII identifier of 1-128 characters.");
  }
  if (identity.displayName.isEmpty() || identity.displayName.size() > 128 ||
      identity.displayName.trimmed() != identity.displayName) {
    throw std::invalid_argument("App name must contain 1-128 characters without surrounding whitespace.");
  }
  for (const auto character : identity.displayName) {
    if (character.unicode() < 0x20 ||
        (character.unicode() >= 0x7f && character.unicode() <= 0x9f) ||
        character.unicode() == 0x2028 || character.unicode() == 0x2029) {
      throw std::invalid_argument("App name cannot contain control or line-separator characters.");
    }
  }
}

inline void addApplicationIdentityOptions(QCommandLineParser& parser) {
  parser.addOption({QStringLiteral("app-id"), QStringLiteral("Reverse-DNS application ID."), QStringLiteral("id")});
  parser.addOption({QStringLiteral("app-name"), QStringLiteral("Application display name."), QStringLiteral("name")});
  parser.addOption({QStringLiteral("component-name"), QStringLiteral("Registered AppRegistry component."), QStringLiteral("name")});
  parser.addOption({QStringLiteral("describe-app"), QStringLiteral("Print process identity without loading JavaScript.")});
}

inline ApplicationIdentity readApplicationIdentity(
    const QCommandLineParser& parser, ApplicationIdentity defaults = {}) {
  int provided = 0;
  for (const auto& option : {QStringLiteral("app-id"), QStringLiteral("app-name"), QStringLiteral("component-name")}) {
    if (parser.values(option).size() > 1) throw std::invalid_argument("Duplicate application identity option.");
    if (parser.isSet(option)) ++provided;
  }
  if (provided == 0) {
    if (defaults.custom) validateApplicationIdentity(defaults);
    return defaults;
  }
  if (provided != 3) throw std::invalid_argument("Supply --app-id, --app-name and --component-name together.");
  ApplicationIdentity identity{parser.value(QStringLiteral("app-id")),
      parser.value(QStringLiteral("app-name")), parser.value(QStringLiteral("component-name")), true};
  validateApplicationIdentity(identity);
  if (defaults.custom && (identity.appId != defaults.appId ||
      identity.displayName != defaults.displayName || identity.componentName != defaults.componentName)) {
    throw std::invalid_argument("A configured consumer executable cannot change its compiled application identity.");
  }
  return identity;
}

inline void applyApplicationIdentity(const ApplicationIdentity& identity) {
  if (!identity.custom) return; // Preserve the original playground defaults.
  validateApplicationIdentity(identity);
  QCoreApplication::setApplicationName(identity.appId);
  QGuiApplication::setApplicationDisplayName(identity.displayName);
  QGuiApplication::setDesktopFileName(identity.appId);
}

inline QByteArray describeApplicationIdentity(const ApplicationIdentity& identity) {
  QJsonArray iconSizes;
  for (const auto& size : QGuiApplication::windowIcon().availableSizes()) iconSizes.append(size.width());
  return QJsonDocument(QJsonObject{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("applicationName"), QCoreApplication::applicationName()},
      {QStringLiteral("displayName"), QGuiApplication::applicationDisplayName()},
      {QStringLiteral("desktopFileName"), QGuiApplication::desktopFileName()},
      {QStringLiteral("componentName"), identity.componentName},
      {QStringLiteral("customIdentity"), identity.custom},
      {QStringLiteral("windowIconSizes"), iconSizes},
      {QStringLiteral("runtimeVerified"), false},
      {QStringLiteral("osAuthorizationVerified"), false}}).toJson(QJsonDocument::Compact);
}

} // namespace rnqnx::runtime
