import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const repo = process.cwd();
const scratch = mkdtempSync(path.join(os.tmpdir(), 'rnqnx-accessibility-action-fixture-'));
const bundle = path.join(scratch, 'host-produced.bundle.js');
writeFileSync(bundle, '// Configuration test input; not a production fixture.\n');
// Use the host compiler only to configure a tiny graph. Switching this local
// variable selects the actual module's QNX branch; it is not a QNX cross-build.
writeFileSync(
  path.join(scratch, 'CMakeLists.txt'),
  `cmake_minimum_required(VERSION 3.20)
project(AccessibilityActionFixtureConfiguration LANGUAGES CXX)
set(BUILD_TESTING OFF)
if(CASE_QNX)
  set(CMAKE_SYSTEM_NAME QNX)
endif()
# This executable is never run: only the generated dependency graph is checked.
set(RNQNX_ACCESSIBILITY_ACTION_TEST_HERMESC "\${CMAKE_COMMAND}" CACHE FILEPATH "" FORCE)
function(rnqnx_enable_strict_warnings target)
endfunction()
add_library(rnqnx_react_host INTERFACE)
add_library(rnqnx_views_qt INTERFACE)
include("${repo}/cmake/RnqnxAccessibilityActions.cmake")
rnqnx_add_accessibility_action_probe()
`
);
/** @param {string} name @param {string[]} args */
function configure(name, args) {
  const build = path.join(scratch, name);
  const result = spawnSync('cmake', ['-S', scratch, '-B', build, '-G', 'Ninja', ...args], {
    encoding: 'utf8',
    timeout: 30000,
  });
  return { build, result, diagnostics: `${result.stdout}\n${result.stderr}` };
}
try {
  const host = configure('host', []);
  assert.equal(host.result.status, 0, host.diagnostics);
  const hostGraph = readFileSync(path.join(host.build, 'build.ninja'), 'utf8');
  assert.match(
    hostGraph,
    /metro"? build accessibility-action-probe\.js --platform qnx --dev false/
  );
  assert.doesNotMatch(hostGraph, /accessibility-action-probe\.hbc/);

  const generated = configure('qnx-generated', ['-DCASE_QNX=ON']);
  assert.equal(generated.result.status, 0, generated.diagnostics);
  const generatedGraph = readFileSync(path.join(generated.build, 'build.ninja'), 'utf8');
  assert.match(
    generatedGraph,
    /metro"? build accessibility-action-probe\.js --platform qnx --dev false/
  );
  assert.match(generatedGraph, /-O -emit-binary -out .*accessibility-action-probe\.hbc/);

  const cached = configure('qnx-cached', [
    '-DCASE_QNX=ON',
    `-DRNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE=${bundle}`,
  ]);
  assert.equal(cached.result.status, 0, cached.diagnostics);
  const cachedGraph = readFileSync(path.join(cached.build, 'build.ninja'), 'utf8');
  assert.doesNotMatch(cachedGraph, /metro"? build/);
  assert.match(cachedGraph, /-O -emit-binary -out .*accessibility-action-probe\.hbc/);
  assert.ok(
    cachedGraph.includes(bundle),
    'Hermes bytecode must depend on the supplied host bundle'
  );
  assert.match(cachedGraph, /RNQNX_ACCESSIBILITY_ACTION_FIXTURE.*accessibility-action-probe\.hbc/);

  for (const [name, input] of [
    ['qnx-missing', path.join(scratch, 'missing.js')],
    ['qnx-directory', scratch],
  ]) {
    const invalid = configure(name, [
      '-DCASE_QNX=ON',
      `-DRNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE=${input}`,
    ]);
    assert.notEqual(invalid.result.status, 0);
    assert.match(
      invalid.diagnostics,
      /RNQNX_ACCESSIBILITY_ACTION_TEST_BUNDLE must name an existing file/
    );
  }
  console.log(
    'AccessibilityAction fixture configuration: normal host/QNX generation, cached HBC dependency and invalid-path rejection passed (host graph only)'
  );
} finally {
  rmSync(scratch, { recursive: true, force: true });
}
