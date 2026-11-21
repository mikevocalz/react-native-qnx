#include "react-native-qnx/runtime/RuntimePreflight.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace rnqnx::runtime {
namespace {

constexpr qsizetype kMaxFileBytes = 1024 * 1024;
constexpr qsizetype kMaxEntries = 256;
constexpr std::size_t kMaxRowsShown = 12;
constexpr auto kRuntimeFile = "rnqnx-runtime.json";
constexpr auto kAppFile = "rnqnx-app.json";
constexpr auto kRuntimeSchema = "rnqnx-runtime/v1";
constexpr auto kAppSchema = "rnqnx-app/v1";

// The stack of three kinds of native module, in the order they are shown.
struct Kind {
  const char* key;
  const char* label;
};
constexpr Kind kKinds[] = {
    {"nitroModules", "Nitro module"},
    {"expoModules", "Expo module"},
    {"fabricComponents", "Fabric component"},
};

struct PackageRef {
  QString package;
  QString version;
};
using PackageList = std::vector<PackageRef>;

// A file that exists but cannot be used. Caught at the top of the check and
// turned into PreflightInvalid; it never leaves this file.
struct BadFile : std::runtime_error {
  using std::runtime_error::runtime_error;
};

[[noreturn]] void bad(const char* file, const QString& what) {
  throw BadFile(QStringLiteral("%1: %2").arg(QString::fromLatin1(file), what).toStdString());
}

QJsonObject parseObject(const QByteArray& bytes, const char* file) {
  if (bytes.size() > kMaxFileBytes) bad(file, QStringLiteral("larger than 1 MiB"));
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    bad(file, QStringLiteral("not a JSON object"));
  return document.object();
}

QString stringField(const QJsonObject& object, const char* key, const char* file) {
  const auto value = object.value(QLatin1String(key));
  if (!value.isString() || value.toString().isEmpty())
    bad(file, QStringLiteral("%1 is missing or not a string").arg(QLatin1String(key)));
  return value.toString();
}

// A string, or JSON null when the app does not use the thing (no Nitro).
std::optional<QString> nullableString(const QJsonObject& object, const char* key,
                                      const char* file) {
  const auto value = object.value(QLatin1String(key));
  if (value.isNull()) return std::nullopt;
  if (!value.isString() || value.toString().isEmpty())
    bad(file, QStringLiteral("%1 must be a string or null").arg(QLatin1String(key)));
  return value.toString();
}

PackageList packageList(const QJsonObject& group, const char* key, const char* file) {
  const auto value = group.value(QLatin1String(key));
  if (!value.isArray()) bad(file, QStringLiteral("%1 must be an array").arg(QLatin1String(key)));
  const auto array = value.toArray();
  if (array.size() > kMaxEntries)
    bad(file, QStringLiteral("%1 has more than %2 entries").arg(QLatin1String(key)).arg(kMaxEntries));
  PackageList list;
  for (const auto& item : array) {
    const auto entry = item.toObject();
    PackageRef ref{stringField(entry, "package", file), stringField(entry, "version", file)};
    const bool repeated = std::any_of(list.begin(), list.end(), [&ref](const PackageRef& other) {
      return other.package == ref.package;
    });
    if (repeated)
      bad(file, QStringLiteral("%1 lists %2 twice").arg(QLatin1String(key), ref.package));
    list.push_back(std::move(ref));
  }
  return list;
}

struct Runtime {
  QString reactNative;
  std::uint32_t hermesBytecode = 0;
  QString nitroAbi;
  PackageList builtins[std::size(kKinds)];
};

struct App {
  std::optional<QString> reactNative;
  std::optional<QString> nitroAbi;
  PackageList needs[std::size(kKinds)];
};

Runtime parseRuntime(const QByteArray& bytes) {
  const auto object = parseObject(bytes, kRuntimeFile);
  if (object.value(QLatin1String("schema")).toString() != QLatin1String(kRuntimeSchema))
    bad(kRuntimeFile, QStringLiteral("unsupported schema, expected %1").arg(QLatin1String(kRuntimeSchema)));
  Runtime runtime;
  runtime.reactNative = stringField(object.value(QLatin1String("versions")).toObject(),
                                    "reactNative", kRuntimeFile);
  const double bytecode = object.value(QLatin1String("hermesBytecodeVersion")).toDouble(-1);
  if (!(bytecode >= 1 && bytecode <= 4294967295.0) || std::floor(bytecode) != bytecode)
    bad(kRuntimeFile, QStringLiteral("hermesBytecodeVersion must be a positive integer"));
  runtime.hermesBytecode = static_cast<std::uint32_t>(bytecode);
  runtime.nitroAbi = stringField(object, "nitroAbi", kRuntimeFile);
  const auto builtins = object.value(QLatin1String("builtins")).toObject();
  for (std::size_t i = 0; i < std::size(kKinds); ++i)
    runtime.builtins[i] = packageList(builtins, kKinds[i].key, kRuntimeFile);
  return runtime;
}

App parseApp(const QByteArray& bytes) {
  const auto object = parseObject(bytes, kAppFile);
  if (object.value(QLatin1String("schema")).toString() != QLatin1String(kAppSchema))
    bad(kAppFile, QStringLiteral("unsupported schema, expected %1").arg(QLatin1String(kAppSchema)));
  App app;
  app.reactNative = nullableString(object, "reactNative", kAppFile);
  app.nitroAbi = nullableString(object, "nitroAbi", kAppFile);
  const auto required = object.value(QLatin1String("requires")).toObject();
  for (std::size_t i = 0; i < std::size(kKinds); ++i)
    app.needs[i] = packageList(required, kKinds[i].key, kAppFile);
  return app;
}

std::vector<PreflightRow> compare(const Runtime& runtime, const App& app,
                                  std::optional<std::uint32_t> bundleBytecode) {
  std::vector<PreflightRow> rows;
  const auto changed = [&rows](const QString& subject, const QString& runner, const QString& appValue) {
    rows.push_back({PreflightRow::Kind::Changed, subject, runner, appValue});
  };
  if (app.reactNative && *app.reactNative != runtime.reactNative)
    changed(QStringLiteral("React Native"), runtime.reactNative, *app.reactNative);
  if (bundleBytecode && *bundleBytecode != runtime.hermesBytecode)
    changed(QStringLiteral("Hermes bytecode"), QString::number(runtime.hermesBytecode),
            QString::number(*bundleBytecode));
  if (app.nitroAbi && *app.nitroAbi != runtime.nitroAbi)
    changed(QStringLiteral("Nitro ABI"), runtime.nitroAbi, *app.nitroAbi);
  // A module is covered only by the same package at the same version: another
  // version is different native code.
  for (std::size_t i = 0; i < std::size(kKinds); ++i) {
    for (const auto& needed : app.needs[i]) {
      const auto& have = runtime.builtins[i];
      const auto found = std::find_if(have.begin(), have.end(), [&needed](const PackageRef& ref) {
        return ref.package == needed.package;
      });
      const auto subject = QStringLiteral("%1 %2").arg(QLatin1String(kKinds[i].label), needed.package);
      if (found == have.end())
        rows.push_back({PreflightRow::Kind::Missing, subject, {}, needed.version});
      else if (found->version != needed.version)
        changed(subject, found->version, needed.version);
    }
  }
  return rows;
}

}  // namespace

PreflightResult checkRuntimePreflight(const std::optional<QByteArray>& runtimeJson,
                                      const std::optional<QByteArray>& appJson,
                                      std::optional<std::uint32_t> bundleBytecode) {
  if (!appJson) return PreflightNotChecked{};
  try {
    if (!runtimeJson)
      bad(kRuntimeFile, QStringLiteral("not found next to the runner, so %1 cannot be checked")
                            .arg(QLatin1String(kAppFile)));
    const auto app = parseApp(*appJson);
    const auto runtime = parseRuntime(*runtimeJson);
    auto rows = compare(runtime, app, bundleBytecode);
    if (rows.empty()) return PreflightCompatible{};
    return PreflightIncompatible{std::move(rows)};
  } catch (const BadFile& error) {
    return PreflightInvalid{QString::fromStdString(error.what())};
  }
}

std::optional<std::uint32_t> hermesBytecodeVersion(const QByteArray& bundle) {
  // hermes lib/BCGen/HBC/BytecodeFileFormat.h: magic 0x1F1903C103BC1FC6.
  static constexpr unsigned char kMagic[8] = {0xc6, 0x1f, 0xbc, 0x03, 0xc1, 0x03, 0x19, 0x1f};
  if (bundle.size() < 12 || std::memcmp(bundle.constData(), kMagic, sizeof kMagic) != 0)
    return std::nullopt;
  const auto* bytes = reinterpret_cast<const unsigned char*>(bundle.constData());
  return static_cast<std::uint32_t>(bytes[8]) | static_cast<std::uint32_t>(bytes[9]) << 8 |
         static_cast<std::uint32_t>(bytes[10]) << 16 | static_cast<std::uint32_t>(bytes[11]) << 24;
}

QString preflightTitle(const PreflightResult& result) {
  if (std::holds_alternative<PreflightInvalid>(result))
    return QStringLiteral("Runner details could not be checked");
  return QStringLiteral("This runner needs a native build");
}

QString preflightMessage(const PreflightResult& result) {
  if (const auto* invalid = std::get_if<PreflightInvalid>(&result))
    return invalid->reason + QStringLiteral("\n\nUse a runner built for this app, or rebuild it from the current project.");
  const auto* incompatible = std::get_if<PreflightIncompatible>(&result);
  if (!incompatible) return {};
  QString text = QStringLiteral(
      "The app requires native modules this runner cannot load. Build the app with its current native dependencies.\n");
  const auto shown = std::min(incompatible->rows.size(), kMaxRowsShown);
  for (std::size_t i = 0; i < shown; ++i) {
    const auto& row = incompatible->rows[i];
    if (row.kind == PreflightRow::Kind::Missing)
      text += QStringLiteral("\nMissing  %1 %2").arg(row.subject, row.app);
    else
      text += QStringLiteral("\nChanged native code  %1  runner %2  app %3").arg(row.subject, row.runner, row.app);
  }
  if (incompatible->rows.size() > shown)
    text += QStringLiteral("\nand %1 more").arg(incompatible->rows.size() - shown);
  text += QStringLiteral("\n\nBuild this app on your development host\n  rnqnx project build-native --project .");
  return text;
}

}  // namespace rnqnx::runtime
