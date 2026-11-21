#pragma once

// Native runner preflight: before any JavaScript runs, compare what the app
// needs (rnqnx-app.json) with what this runner was built with
// (rnqnx-runtime.json). Pure data in, a value out; no Qt event loop, no file
// access, no JavaScript runtime. Contract: docs/nitro/API_CONTRACTS.md,
// "S6 native runner preflight". Text: `preflight.*` in docs/v1/design/strings.md.

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace rnqnx::runtime {

struct PreflightRow {
  enum class Kind { Missing, Changed };
  Kind kind;
  // "Nitro module nitro-foo", "React Native", "Hermes bytecode", ...
  QString subject;
  // `app` is the version the app needs. `runner` is empty for Missing and
  // the runner's own value for Changed. Neither side is ever shortened.
  QString runner;
  QString app;
};

// Nothing to compare: the app did not ship rnqnx-app.json. Not a pass.
struct PreflightNotChecked {};
struct PreflightCompatible {};
struct PreflightIncompatible {
  std::vector<PreflightRow> rows;  // never empty
};
// A file was present and unusable. `reason` names the file and the problem.
struct PreflightInvalid {
  QString reason;
};

using PreflightResult = std::variant<PreflightNotChecked, PreflightCompatible,
                                     PreflightIncompatible, PreflightInvalid>;

// `runtimeJson` is the runner's rnqnx-runtime.json, `appJson` the app's
// rnqnx-app.json; nullopt means the file does not exist. `bundleBytecode` is
// the bundle's Hermes bytecode version when the bundle is bytecode.
// An app file without a usable runner file is Invalid, never Compatible.
PreflightResult checkRuntimePreflight(const std::optional<QByteArray>& runtimeJson,
                                      const std::optional<QByteArray>& appJson,
                                      std::optional<std::uint32_t> bundleBytecode);

// Version in the Hermes bytecode header (64-bit magic, 32-bit version, both
// little-endian), or nullopt for anything else, including a JavaScript bundle.
std::optional<std::uint32_t> hermesBytecodeVersion(const QByteArray& bundle);

// Screen text for an Incompatible or Invalid result: `preflight.title` or
// `preflight.invalidTitle`, and the body with one line per row.
QString preflightTitle(const PreflightResult& result);
QString preflightMessage(const PreflightResult& result);

}  // namespace rnqnx::runtime
