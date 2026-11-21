// Checks the native runner preflight against the fixtures the JavaScript
// tests also use. A pass says the comparison and its text keep their contract
// on this host. It says nothing about a runner on QNX or a device.

#include "react-native-qnx/runtime/RuntimePreflight.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtGlobal>

#include <cstdio>
#include <cstdlib>

using namespace rnqnx::runtime;

#define CHECK(cond)                                                           \
  do {                                                                        \
    if (!(cond)) {                                                            \
      std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #cond); \
      std::exit(1);                                                           \
    }                                                                         \
  } while (false)

namespace {

// RNQNX_PREFLIGHT_FIXTURES_DIR names the fixtures on a target that has no copy
// of the build tree (the QNX VM); otherwise the path compiled in is used.
QByteArray fixture(const char* name) {
  const auto directory = qEnvironmentVariableIsSet("RNQNX_PREFLIGHT_FIXTURES_DIR")
      ? qEnvironmentVariable("RNQNX_PREFLIGHT_FIXTURES_DIR")
      : QStringLiteral(RNQNX_PREFLIGHT_FIXTURES);
  QFile file(directory + QStringLiteral("/") + QLatin1String(name));
  CHECK(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

template <class T>
const T* as(const PreflightResult& result) {
  return std::get_if<T>(&result);
}

QByteArray replaced(QByteArray bytes, const char* from, const char* to) {
  CHECK(bytes.contains(from));
  return bytes.replace(from, to);
}

// The case this slice exists for: a stock runner that lacks a module the app
// adds must be rejected before evaluation, and a runner built with it accepts.
void testAddedModule() {
  const auto app = fixture("app-added-module.json");
  const auto stock = checkRuntimePreflight(fixture("runtime-stock.json"), app, std::nullopt);
  const auto* rejected = as<PreflightIncompatible>(stock);
  CHECK(rejected);
  CHECK(rejected->rows.size() == 1);
  CHECK(rejected->rows[0].kind == PreflightRow::Kind::Missing);
  CHECK(rejected->rows[0].subject == QStringLiteral("Nitro module fixture-added-nitro"));
  CHECK(rejected->rows[0].app == QStringLiteral("2.0.0"));
  const auto custom = checkRuntimePreflight(fixture("runtime-custom.json"), app, 99u);
  CHECK(as<PreflightCompatible>(custom));
}

void testChanged() {
  const auto stock = fixture("runtime-stock.json");
  const auto app = replaced(fixture("app-added-module.json"), "\"fixture-expo\", \"version\": \"1.0.0\"",
                            "\"fixture-expo\", \"version\": \"1.1.0\"");
  // The added Nitro module is missing and the Expo module differs: both rows.
  const auto result = checkRuntimePreflight(stock, app, std::nullopt);
  const auto* rejected = as<PreflightIncompatible>(result);
  CHECK(rejected && rejected->rows.size() == 2);
  CHECK(rejected->rows[1].kind == PreflightRow::Kind::Changed);
  CHECK(rejected->rows[1].subject == QStringLiteral("Expo module fixture-expo"));
  CHECK(rejected->rows[1].runner == QStringLiteral("1.0.0"));
  CHECK(rejected->rows[1].app == QStringLiteral("1.1.0"));
}

void testRuntimeFields() {
  const auto custom = fixture("runtime-custom.json");
  const auto app = fixture("app-added-module.json");
  const auto rn = checkRuntimePreflight(custom, replaced(app, "\"reactNative\": \"0.88.1\"", "\"reactNative\": \"0.87.0\""), 99u);
  CHECK(as<PreflightIncompatible>(rn) && as<PreflightIncompatible>(rn)->rows[0].subject == QStringLiteral("React Native"));
  const auto abi = checkRuntimePreflight(custom, replaced(app, "nightly.136", "nightly.137"), 99u);
  CHECK(as<PreflightIncompatible>(abi) && as<PreflightIncompatible>(abi)->rows[0].subject == QStringLiteral("Nitro ABI"));
  const auto hbc = checkRuntimePreflight(custom, app, 98u);
  CHECK(as<PreflightIncompatible>(hbc) && as<PreflightIncompatible>(hbc)->rows[0].subject == QStringLiteral("Hermes bytecode"));
  // null means the app does not use Nitro or React Native metadata: not compared.
  const auto nulls = checkRuntimePreflight(
      custom, replaced(replaced(app, "\"reactNative\": \"0.88.1\"", "\"reactNative\": null"),
                       "\"nitroAbi\": \"1.0.0-alpha.nightly.136.gdb81de61bd32\"", "\"nitroAbi\": null"), std::nullopt);
  CHECK(as<PreflightCompatible>(nulls));
}

void testNoAppFile() {
  // Without rnqnx-app.json nothing is known about the app. Not a pass.
  CHECK(as<PreflightNotChecked>(checkRuntimePreflight(fixture("runtime-stock.json"), std::nullopt, std::nullopt)));
  CHECK(as<PreflightNotChecked>(checkRuntimePreflight(std::nullopt, std::nullopt, std::nullopt)));
}

void testInvalid() {
  const auto app = fixture("app-added-module.json");
  const auto stock = fixture("runtime-stock.json");
  const auto missing = checkRuntimePreflight(std::nullopt, app, std::nullopt);
  CHECK(as<PreflightInvalid>(missing) && as<PreflightInvalid>(missing)->reason.startsWith(QStringLiteral("rnqnx-runtime.json:")));
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(stock, QByteArray("not json"), std::nullopt)));
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(QByteArray("[]"), app, std::nullopt)));
  const auto schema = checkRuntimePreflight(replaced(stock, "rnqnx-runtime/v1", "rnqnx-runtime/v2"), app, std::nullopt);
  CHECK(as<PreflightInvalid>(schema) && as<PreflightInvalid>(schema)->reason.contains(QStringLiteral("unsupported schema")));
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(stock, replaced(app, "rnqnx-app/v1", "other"), std::nullopt)));
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(replaced(stock, "\"hermesBytecodeVersion\": 99", "\"hermesBytecodeVersion\": 0"), app, std::nullopt)));
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(replaced(stock, "\"hermesBytecodeVersion\": 99", "\"hermesBytecodeVersion\": 1.5"), app, std::nullopt)));
  // A missing key is not null: the app file must say what it does not use.
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(stock, replaced(app, "\"reactNative\": \"0.88.1\",", ""), std::nullopt)));
  // A repeated package makes "same package, same version" ambiguous.
  const auto twice = replaced(app, "\"expoModules\": [{ \"package\": \"fixture-expo\", \"version\": \"1.0.0\" }]",
                              "\"expoModules\": [{ \"package\": \"x\", \"version\": \"1\" }, { \"package\": \"x\", \"version\": \"2\" }]");
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(stock, twice, std::nullopt)));
  CHECK(as<PreflightInvalid>(checkRuntimePreflight(stock, QByteArray(1024 * 1024 + 1, ' '), std::nullopt)));
}

void testBytecodeHeader() {
  const auto header = QByteArray::fromHex("c61fbc03c103191f63000000");
  CHECK(hermesBytecodeVersion(header) == 99u);
  CHECK(hermesBytecodeVersion(QByteArray::fromHex("c61fbc03c103191f010203ff")) == 0xff030201u);
  CHECK(!hermesBytecodeVersion(header.left(11)));
  CHECK(!hermesBytecodeVersion(QByteArray("globalThis.answer = 42;")));
  CHECK(!hermesBytecodeVersion({}));
}

void testText() {
  const auto result = checkRuntimePreflight(fixture("runtime-stock.json"), fixture("app-added-module.json"), std::nullopt);
  CHECK(preflightTitle(result) == QStringLiteral("This runner needs a native build"));
  const auto text = preflightMessage(result);
  CHECK(text.contains(QStringLiteral("Missing  Nitro module fixture-added-nitro 2.0.0")));
  CHECK(text.contains(QStringLiteral("rnqnx project build-native --project .")));
  const auto invalid = checkRuntimePreflight(std::nullopt, fixture("app-added-module.json"), std::nullopt);
  CHECK(preflightTitle(invalid) == QStringLiteral("Runner details could not be checked"));
  CHECK(preflightMessage(invalid).contains(QStringLiteral("rnqnx-runtime.json")));
  CHECK(preflightMessage(PreflightCompatible{}).isEmpty());

  // More rows than the screen shows: the count of the rest is stated.
  QByteArray many = "{\"schema\":\"rnqnx-app/v1\",\"reactNative\":null,\"nitroAbi\":null,\"requires\":{\"nitroModules\":[";
  for (int i = 0; i < 20; ++i) many += (i ? "," : "") + QByteArray("{\"package\":\"m") + QByteArray::number(i) + "\",\"version\":\"1\"}";
  many += "],\"expoModules\":[],\"fabricComponents\":[]}}";
  const auto crowded = checkRuntimePreflight(fixture("runtime-stock.json"), many, std::nullopt);
  CHECK(as<PreflightIncompatible>(crowded) && as<PreflightIncompatible>(crowded)->rows.size() == 20);
  CHECK(preflightMessage(crowded).contains(QStringLiteral("and 8 more")));
}

}  // namespace

int main() {
  testAddedModule();
  testChanged();
  testRuntimeFields();
  testNoAppFile();
  testInvalid();
  testBytecodeHeader();
  testText();
  std::printf("runtime preflight: all checks passed\n");
  return 0;
}
