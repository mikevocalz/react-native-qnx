import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { readFile, readdir } from 'node:fs/promises';

/** @param {string} file */
const read = (file) => readFile(file, 'utf8');
const cmake = await read('CMakeLists.txt');
const native = await read('cmake/RnqnxMarathonHost.cmake');
const host = await read('vnext/probes/playground/main.cpp');
const runtimeHpp = await read('native/platform/linux-host/nitro/MarathonNitroRuntime.hpp');
const runtimeCpp = await read('native/platform/linux-host/nitro/MarathonNitroRuntime.cpp');
const nitroCmake = await read('native/platform/linux-host/nitro/CMakeLists.txt');
const workflow = await read('.github/workflows/marathon-qt-host.yml');
const nitroWorkflow = await read('.github/workflows/nitro-v1-host.yml');

/** CMake text with comment lines removed, so prose can mention forbidden names. */
const activeLines = (text) =>
  text
    .split('\n')
    .filter((line) => !line.trimStart().startsWith('#'))
    .join('\n');
const activeNative = activeLines(native);

// Build entry points.
assert.match(cmake, /option\(RNQNX_BUILD_MARATHON_HOST[\s\S]*?OFF\)/);
assert.match(cmake, /option\(RNQNX_MARATHON_HOST_NITRO[\s\S]*?OFF\)/);
assert.match(cmake, /CMAKE_SYSTEM_NAME STREQUAL "Linux"/);
assert.match(cmake, /if\(RNQNX_BUILD_REACT_QNX\)/);
assert.match(
  cmake,
  /RNQNX_MARATHON_HOST_NITRO AND NOT RNQNX_ENABLE_NITRO_QT_DISPATCHER[\s\S]*?FATAL_ERROR/,
  'Marathon Nitro must refuse to configure without the Qt dispatcher'
);
assert.match(native, /target_link_libraries\(rnqnx_marathon_host PRIVATE rnqnx_react_host\)/);
assert.match(native, /--platform marathon --dev false/);
assert.match(native, /metro\.marathon\.config\.js/);
assert.match(
  native,
  /DEPENDS[\s\S]*?apps\/rn-playground\/qnx-asset\.png/,
  'Metro must rebuild the Marathon bundle when qnx-asset.png changes'
);
assert.match(native, /NOT TARGET rnqnx_react_host/);
assert.doesNotMatch(native, /Requires the Marathon Metro PR|first executable slice/);

// Nitro wiring: the host links the Marathon adapter and the shared Qt
// dispatcher, and defines the macro main.cpp keys the install on.
assert.match(
  activeNative,
  /add_subdirectory\([^)]*native\/platform\/linux-host\/nitro/,
  'the Marathon Nitro adapter must be added to the host build'
);
assert.match(
  activeNative,
  /target_link_libraries\(rnqnx_marathon_host PRIVATE\s+rnqnx_nitro_marathon rnqnx_nitro_qt\)/
);
assert.match(activeNative, /RNQNX_MARATHON_NITRO=1/);
assert.match(activeNative, /RNQNX_MARATHON_HOST_NITRO AND NOT TARGET rnqnx_nitro_qt/);

// No QNX-only symbol in the Marathon link graph.
assert.doesNotMatch(
  activeNative,
  /libscreen|qnx_platform|QNX_SDP_ENV|rnqnx_nitro_qnx|QnxPlatformInfo|installQnxNitroRuntime/
);
assert.doesNotMatch(
  activeLines(nitroCmake),
  /libscreen|qnx_platform|rnqnx_nitro_qnx|QnxPlatformInfo|qnx-nitro-runtime\/cpp\/Qnx/
);
assert.match(activeLines(nitroCmake), /MarathonNitroRuntime\.cpp/);
assert.match(activeLines(nitroCmake), /MarathonNitroProof\.cpp/);
for (const file of [
  'MarathonNitroRuntime.cpp',
  'MarathonNitroRuntime.hpp',
  'MarathonNitroProof.cpp',
  'MarathonNitroProof.hpp',
  'MarathonPlatformInfo.cpp',
  'MarathonPlatformInfo.hpp',
]) {
  const source = await read(`native/platform/linux-host/nitro/${file}`);
  assert.doesNotMatch(
    source,
    /QnxPlatformInfo|installQnxNitroRuntime|<screen\/|libscreen/,
    `${file} must not reference QNX-only symbols`
  );
}
assert.match(
  runtimeHpp,
  /installMarathonNitroRuntime\(\s*facebook::jsi::Runtime& runtime,\s*std::shared_ptr<margelo::nitro::Dispatcher> dispatcher\)/
);
assert.match(runtimeCpp, /std::invalid_argument/);
assert.match(runtimeCpp, /installRuntimeGlobalDispatcher/);
assert.doesNotMatch(runtimeHpp + runtimeCpp, /NOT connected|Deliberately no async dispatcher/);

// Host source: identity, install call with the Qt dispatcher, proof flag.
assert.match(host, /#ifdef RNQNX_MARATHON_HOST/);
assert.match(host, /org\.rnqnx\.marathon\.playground/);
assert.match(host, /setDesktopFileName/);
assert.match(host, /#ifdef RNQNX_MARATHON_NITRO/);
assert.match(
  host,
  /installMarathonNitroRuntime\(\s*host\.runtime\(\),\s*std::make_shared<rnqnx::nitro::QtNitroDispatcher>\(host\.taskQueue\(\)\)\)/,
  'the host must install Marathon Nitro with the Qt dispatcher over its task queue'
);
assert.match(host, /nitro-proof/);
assert.match(host, /RNQNX_MARATHON_NITRO_OK/);
assert.match(host, /RNQNX_MARATHON_NITRO_FAILED/);
const marathonNitroBlocks = [...host.matchAll(/#ifdef RNQNX_MARATHON_NITRO[\s\S]*?#endif/g)]
  .map((match) => match[0])
  .join('\n');
assert.doesNotMatch(marathonNitroBlocks, /Qnx(NitroRuntime|PlatformInfo)|installQnxNitroRuntime/);

// Workflow: widened filters, push to main, Nitro runtime proof, Wayland proof.
const pathLists = [...workflow.matchAll(/^ {4}paths:\n((?: {6}- '.*'\n)+)/gm)].map((match) =>
  [...match[1].matchAll(/'(.*)'/g)].map((entry) => entry[1])
);
assert.equal(pathLists.length, 2, 'pull_request and push must both filter paths');
assert.deepEqual(pathLists[0], pathLists[1], 'pull_request and push path filters must match');
for (const required of [
  'vnext/**',
  'cmake/**',
  'native/platform/linux-host/**',
  'packages/qnx-nitro-runtime/**',
  'apps/rn-playground/**',
  'pnpm-lock.yaml',
  'CMakeLists.txt',
  'tests/native/marathon-wayland/**',
  '.github/workflows/marathon-qt-host.yml',
]) {
  assert.ok(pathLists[0].includes(required), `workflow paths must include ${required}`);
}
assert.match(workflow, /push:\n {4}branches: \[main\]/);
assert.match(workflow, /workflow_dispatch:/);
assert.match(workflow, /-DRNQNX_ENABLE_NITRO_QT_DISPATCHER=ON/);
assert.match(workflow, /-DRNQNX_MARATHON_HOST_NITRO=ON/);
assert.match(workflow, /-DRNQNX_NITRO_CORE_ROOT=/);
assert.match(workflow, /--nitro-proof/);
assert.match(workflow, /RNQNX_MARATHON_NITRO_OK platform=marathon/);
assert.match(workflow, /-R '\^rnqnx_nitro_'/);
assert.match(workflow, /libqwayland\*\.so/);
assert.match(workflow, /weston --backend=headless/);
assert.match(workflow, /QT_QPA_PLATFORM: wayland/);
assert.match(workflow, /WAYLAND_DEBUG=client/);
assert.match(workflow, /check-wayland-client-log\.py/);
assert.match(workflow, /not a MarathonOS device proof/);
assert.doesNotMatch(workflow, /\bsleep [1-9]\d*\b(?!\.)/, 'no fixed sleeps');

// Every action is pinned by commit SHA, and the SHAs match nitro-v1-host.yml.
const uses = [...workflow.matchAll(/^\s*(?:- )?uses: (\S+)(?: # (\S+))?$/gm)];
assert.ok(uses.length >= 4);
for (const [, ref, comment] of uses) {
  assert.match(ref, /^[\w.-]+\/[\w.-]+@[0-9a-f]{40}$/, `action must be pinned by SHA: ${ref}`);
  assert.match(comment ?? '', /^v\d+\.\d+\.\d+$/, `pinned action needs a version comment: ${ref}`);
}
for (const action of ['actions/checkout', 'actions/setup-node', 'actions/upload-artifact']) {
  const pin = (text) => text.match(new RegExp(`${action}@([0-9a-f]{40})`))?.[1];
  assert.ok(pin(workflow), `${action} is not pinned in the Marathon workflow`);
  assert.equal(pin(workflow), pin(nitroWorkflow), `${action} pin differs from nitro-v1-host.yml`);
}
assert.match(workflow, /jurplel\/install-qt-action@[0-9a-f]{40} # v4\.\d+\.\d+/);

// The Wayland log checker: accepts a real-shaped log, rejects a wrong app id
// and a toplevel that never presented a buffer.
const checker = 'tests/native/marathon-wayland/check-wayland-client-log.py';
const fixtures = 'tests/native/marathon-wayland/fixtures';
assert.deepEqual((await readdir(fixtures)).sort(), [
  'client-no-buffer.log',
  'client-ok.log',
  'client-wrong-app-id.log',
]);
const accepted = execFileSync('python3', ['-I', checker, `${fixtures}/client-ok.log`], {
  encoding: 'utf8',
});
assert.match(accepted, /^PASS /);
for (const [name, message] of [
  ['client-wrong-app-id.log', /no set_app_id/],
  ['client-no-buffer.log', /never had a wl_buffer attached/],
]) {
  const rejected = spawnSync('python3', ['-I', checker, `${fixtures}/${name}`], {
    encoding: 'utf8',
  });
  assert.equal(rejected.status, 1, `${name} must be rejected`);
  assert.match(rejected.stderr, message);
}

console.log('marathon-native-host-contract.test.mjs passed');
