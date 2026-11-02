import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

/** @param {string} filename */
const read = (filename) => readFile(filename, 'utf8');
const runtime = await read('packages/qnx-nitro-runtime/cpp/QnxNitroRuntime.cpp');
const info = await read('packages/qnx-nitro-runtime/cpp/generated/QnxPlatformInfo.binding.cpp');
const header = await read('packages/qnx-nitro-runtime/cpp/QnxPlatformInfo.hpp');
const js = await read('packages/qnx-nitro-runtime/index.js');
const types = await read('packages/qnx-nitro-runtime/index.d.ts');
const logger = await read('packages/qnx-nitro-runtime/cpp/NitroLogger.cpp');
const thread = await read('packages/qnx-nitro-runtime/cpp/ThreadUtils.cpp');
const codegenRules = await read('docs/nitro/API_CONTRACTS.md');

assert.match(runtime, /HybridClassRegistry::registerHybridClass/);
assert.match(runtime, /margelo::nitro::install\(runtime\)/);
assert.doesNotMatch(runtime, /HybridObjectRegistry|install\(runtime,/);
assert.match(info, /HybridClass::create<Class>/);
assert.match(info, /registerHybridConstructor/);
for (const prop of ['platform', 'architecture']) {
  assert.match(info, new RegExp('registerHybridProperty\\("' + prop + '"'));
  assert.match(types, new RegExp('readonly ' + prop + ':'));
}
assert.match(info, /registerHybridMethod\("add"/);
assert.match(types, /add\(left: number, right: number\): number/);
assert.match(header, /using Base = margelo::nitro::HybridObject::Class/);
assert.match(js, /require\('nitro-core'\)/);
assert.match(js, /NitroModules.getHybridClass\('QnxPlatformInfo'\)/);
assert.match(js, /return new QnxPlatformInfo\(\)/);
assert.doesNotMatch(js, /createHybridObject\(/);
assert.match(logger, /#include <NitroModules\/NitroLogger\.hpp>/);
assert.match(thread, /#include <NitroModules\/ThreadUtils\.hpp>/);
assert.match(codegenRules, /platformSchema = z.enum\(\['apple', 'android'\]\)/);
console.log('Nitro API/JS/native source contracts passed (NOT compiled or device-tested)');
