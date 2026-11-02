import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

/** @param {string} filename */
const source = (filename) => readFile(filename, 'utf8');

const core = await source('cmake/RnqnxNitroV1.cmake');
const qnx = await source('packages/qnx-nitro-runtime/CMakeLists.txt');
const marathon = await source('native/platform/linux-host/nitro/CMakeLists.txt');
const qnxClass = await source('packages/qnx-nitro-runtime/cpp/generated/QnxPlatformInfo.binding.cpp');
const marathonClass = await source('native/platform/linux-host/nitro/generated/MarathonPlatformInfo.binding.cpp');
const runtime = await source('native/platform/linux-host/nitro/MarathonNitroRuntime.cpp');
const tests = await source('tests/native/nitro-v1-host/CMakeLists.txt');
const workflow = await source('.github/workflows/nitro-v1-host.yml');

assert.match(core, /add_library\(rnqnx_nitro_core STATIC/);
assert.match(core, /function\(rnqnx_configure_nitro_v1_adapter target\)/);
assert.doesNotMatch(core, /qnx_platform|libscreen|WaylandClient/);
assert.match(qnx, /add_library\(rnqnx_nitro_qnx STATIC/);
assert.match(qnx, /rnqnx_configure_nitro_v1_adapter\(rnqnx_nitro_qnx\)/);
assert.doesNotMatch(qnx, /MarathonNitroRuntime/);
assert.match(marathon, /add_library\(rnqnx_nitro_marathon STATIC/);
assert.match(marathon, /CMAKE_SYSTEM_NAME STREQUAL "Linux"/);
assert.match(marathon, /rnqnx_configure_nitro_v1_adapter\(rnqnx_nitro_marathon\)/);
assert.doesNotMatch(marathon, /QnxNitroRuntime|libscreen|qnx_platform/);
assert.match(qnxClass, /QnxPlatformInfo::Class::get/);
assert.match(marathonClass, /MarathonPlatformInfo::Class::get/);
assert.match(runtime, /HybridClassRegistry::registerHybridClass/);
assert.match(runtime, /margelo::nitro::install\(runtime\)/);
assert.doesNotMatch(runtime, /install\(runtime,|HybridObjectRegistry/);
assert.match(tests, /add_subdirectory\(.*native\/platform\/linux-host\/nitro/);
assert.match(workflow, /--target rnqnx_nitro_qnx rnqnx_nitro_marathon/);
console.log('nitro-dual-host-contract.test.mjs passed (source-check only)');
