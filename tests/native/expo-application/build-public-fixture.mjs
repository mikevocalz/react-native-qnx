// The official SDK module is transpiled, never reimplemented. Narrow framework
// shims isolate the metadata API from unrelated UI/Expo startup side effects.
import { createRequire } from 'node:module';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import ts from '@typescript/typescript6';

const require = createRequire(import.meta.url);
const applicationRoot =
  process.env.RNQNX_EXPO_APPLICATION_FIXTURE_ROOT ||
  dirname(require.resolve('expo-application/package.json'));
const expoRoot = dirname(require.resolve('expo/package.json'));
const coreRoot = dirname(
  createRequire(join(expoRoot, 'package.json')).resolve('expo-modules-core/package.json')
);
const version = JSON.parse(readFileSync(join(applicationRoot, 'package.json'), 'utf8')).version;
if (version !== '58.1.2')
  throw new Error(`Expected audited expo-application58.1.2, received ${version}`);
const sources = {
  application: join(applicationRoot, 'src/Application.ts'),
  native: join(applicationRoot, 'src/ExpoApplication.ts'),
  types: join(applicationRoot, 'src/Application.types.ts'),
  acquisition: join(coreRoot, 'src/requireNativeModule.ts'),
  install: join(coreRoot, 'src/ensureNativeModulesAreInstalled.native.ts'),
  unavailable: join(coreRoot, 'src/errors/UnavailabilityError.ts'),
  coded: join(coreRoot, 'src/errors/CodedError.ts'),
};
const entries = Object.entries(sources).map(([name, file]) => {
  const output = ts.transpileModule(readFileSync(file, 'utf8'), {
    fileName: file,
    compilerOptions: { target: ts.ScriptTarget.ES2020, module: ts.ModuleKind.CommonJS },
  }).outputText;
  return `${JSON.stringify(name)}: function(require, module, exports) {\n${output}\n}`;
});
const bundle = `(function() {
  var factories = {${entries.join(',\n')}};
  var cache = {};
  var platform = {OS: 'qnx'};
  var framework = {
    'react-native': {TurboModuleRegistry: {get: function() { throw new Error('Unexpected TurboModule fallback'); }}},
    './NativeModulesProxy': {default: {}},
    './TurboModuleToExpoModuleProxy': {createTurboModuleToExpoProxy: function() { throw new Error('Unexpected bridge fallback'); }},
    '../Platform': {default: platform},
  };
  var aliases = {'./Application.types': 'types', './ExpoApplication': 'native', './ensureNativeModulesAreInstalled': 'install', './CodedError': 'coded'};
  function load(id) {
    if (id === 'expo') return {
      requireNativeModule: load('acquisition').requireNativeModule,
      UnavailabilityError: load('unavailable').UnavailabilityError,
      Platform: platform,
    };
    if (framework[id]) return framework[id];
    id = aliases[id] || id;
    if (cache[id]) return cache[id].exports;
    if (!factories[id]) throw new Error('Unexpected official SDK dependency: ' + id);
    var module = cache[id] = {exports: {}};
    factories[id](load, module, module.exports);
    return module.exports;
  }
  globalThis.__expoApplicationPublic = load('application');
})();\n`;
writeFileSync(resolve(process.argv[2]), bundle);
