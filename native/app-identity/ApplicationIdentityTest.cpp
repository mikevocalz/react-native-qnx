#include "react-native-qnx/runtime/ApplicationIdentity.h"
#include <QtTest>

using namespace rnqnx::runtime;

static ApplicationIdentity parse(QStringList args, ApplicationIdentity defaults = {}) {
  QCommandLineParser parser;
  addApplicationIdentityOptions(parser);
  args.prepend(QStringLiteral("identity-test"));
  if (!parser.parse(args)) throw std::invalid_argument("Invalid arguments");
  return readApplicationIdentity(parser, defaults);
}

class ApplicationIdentityTest final : public QObject {
  Q_OBJECT
 private slots:
  void legacyDefaults() {
    QCoreApplication::setApplicationName("legacy");
    const auto identity = parse({});
    QVERIFY(!identity.custom);
    QCOMPARE(identity.componentName, QStringLiteral("RNQnxPlayground"));
    applyApplicationIdentity(identity);
    QCOMPARE(QCoreApplication::applicationName(), QStringLiteral("legacy"));
  }
  void publicIdentity() {
    const auto identity = parse({"--app-id", "org.rnqnx.notes", "--app-name", "Notes & sketches", "--component-name", "FieldNotes"});
    applyApplicationIdentity(identity);
    QCOMPARE(QCoreApplication::applicationName(), identity.appId);
    QCOMPARE(QGuiApplication::desktopFileName(), identity.appId);
    QCOMPARE(QGuiApplication::applicationDisplayName(), identity.displayName);
    const auto json = QJsonDocument::fromJson(describeApplicationIdentity(identity)).object();
    QCOMPARE(json.value("componentName").toString(), QStringLiteral("FieldNotes"));
    QVERIFY(!json.value("runtimeVerified").toBool());
    QVERIFY(!json.value("osAuthorizationVerified").toBool());
  }
  void incompleteAndDuplicateRejected() {
    QVERIFY_EXCEPTION_THROWN(parse({"--app-id", "org.example.app"}), std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(parse({"--app-id", "org.example.app", "--app-id", "org.example.other", "--app-name", "Notes", "--component-name", "Notes"}), std::invalid_argument);
  }
  void invalidIds() {
    for (const auto& id : {"app", "../app", "org.example.app.desktop", "org..app", "org.example.1app", "org.example.a/b", "org.example.app\n"}) {
      QVERIFY_EXCEPTION_THROWN((validateApplicationIdentity({id, "Notes", "Notes", true})), std::invalid_argument);
    }
  }
  void invalidNamesAndComponents() {
    QVERIFY_EXCEPTION_THROWN((validateApplicationIdentity({"org.example.app", " Notes", "Notes", true})), std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN((validateApplicationIdentity({"org.example.app", "Notes\nInjected", "Notes", true})), std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN((validateApplicationIdentity({"org.example.app", QString(129, 'a'), "Notes", true})), std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN((validateApplicationIdentity({"org.example.app", "Notes", "Bad Name", true})), std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN((validateApplicationIdentity({"org.example.app", "Notes", QString(129, 'a'), true})), std::invalid_argument);
  }
  void unicodeDisplayName() {
    ApplicationIdentity identity{"org.example.notes", QString::fromUtf8("Notas — 学習"), "Notes", true};
    validateApplicationIdentity(identity);
    applyApplicationIdentity(identity);
    QCOMPARE(QGuiApplication::applicationDisplayName(), identity.displayName);
  }
  void compiledDefaultsCannotBeChanged() {
    ApplicationIdentity compiled{"org.example.notes", "Notes", "FieldNotes", true};
    QCOMPARE(parse({}, compiled).componentName, compiled.componentName);
    QVERIFY_EXCEPTION_THROWN(parse({"--app-id", "org.example.other", "--app-name", "Notes", "--component-name", "FieldNotes"}, compiled), std::invalid_argument);
    QCOMPARE(parse({"--app-id", "org.example.notes", "--app-name", "Notes", "--component-name", "FieldNotes"}, compiled).appId, compiled.appId);
  }
};
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  ApplicationIdentityTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "ApplicationIdentityTest.moc"
