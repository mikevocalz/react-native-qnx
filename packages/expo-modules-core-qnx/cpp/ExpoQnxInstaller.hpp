#pragma once

#include <jsi/jsi.h>

#include <string>

namespace rnqnx::expo {

/**
 * Installs `globalThis.expo` before the bundle runs: the `modules` object with
 * one instance per registered module, plus the native members Expo Modules
 * Core 58 reads (uuidv4, uuidv5, getViewConfig, reloadAppAsync,
 * expoModulesCoreVersion). The JS classes (EventEmitter, NativeModule,
 * SharedObject, SharedRef) are added by the QNX polyfill that Metro bundles in
 * place of expo-modules-core's native no-op polyfill.
 *
 * Each module is created once per runtime. A dev-client reload creates a new
 * runtime and calls this again; no JSI value outlives its runtime.
 */
void installExpoModulesQnx(facebook::jsi::Runtime& runtime);

/**
 * The app's Expo config (the `expo` object of app.json after config
 * resolution) as JSON, set by the consumer build before installation. Modules
 * such as the expo-constants port read it. Empty when the build embedded none.
 */
void setAppManifestJson(std::string json);
const std::string& appManifestJson();

/** Whether this is a development build (dev client); expo-constants reports
 * it as `debugMode`. Set by the consumer build before installation. */
void setDebugBuild(bool debug);
bool debugBuild();

} // namespace rnqnx::expo
