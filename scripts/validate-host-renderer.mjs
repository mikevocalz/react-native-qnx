#!/usr/bin/env node

import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

// This lane must fail closed: CTest success with only the two smoke tests is
// not evidence that Qt, Fabric, the current Hermes source or Metro ran.
const preset = 'linux-host-renderer';
const build = path.resolve('build', preset);
const expectedTests = [
  'rnqnx_g0_cxx20_probe',
  'qnx_screen_surface_host_test',
  'rnqnx_hermes_probe',
  'qt_react_host_host_test',
  'rnqnx_consumer_window_input',
  'rnqnx_consumer_window_rejects_interception',
  'rnqnx_wheel_direction',
  'rnqnx_component_gallery',
  'rnqnx_ControlledSwitch_test',
  'rnqnx_rn_headers_probe',
  'shadow_view_mutation_adapter_host_test',
  'shadow_tree_mounting_host_test',
  'qtquick_mounting_layer_host_test',
  'rnqnx_qtquick_host_probe',
];

/** @param {string} command @param {string[]} args @param {boolean} capture */
function run(command, args, capture = false) {
  const result = spawnSync(command, args, {
    encoding: 'utf8',
    stdio: capture ? 'pipe' : 'inherit',
    env: {
      ...process.env,
      QT_QPA_PLATFORM: 'offscreen',
      QSG_RHI_BACKEND: 'software',
      QT_QUICK_BACKEND: 'software',
    },
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    if (capture) process.stderr.write(result.stderr ?? '');
    throw new Error(`${command} failed (${result.signal ?? result.status})`);
  }
  return result.stdout;
}

if (process.platform !== 'linux') {
  throw new Error('The reproducible full-renderer lane currently targets Linux.');
}
run('cmake', ['--preset', preset]);
const cache = readFileSync(path.join(build, 'CMakeCache.txt'), 'utf8');
if (/^RNQNX_ENABLE_DESKTOP_WINDOW:BOOL=ON$/m.test(cache)) {
  expectedTests.push(
    'rnqnx_consumer_desktop_window_input',
    'rnqnx_desktop_window',
    'rnqnx_component_gallery_desktop'
  );
}
if (/^RNQNX_ENABLE_TEXT_INPUT_CONTRACT:BOOL=ON$/m.test(cache)) {
  expectedTests.push(
    'rnqnx_text_input_probe',
    'rnqnx_text_input_bridge_test',
    'rnqnx_text_input_rejects_interception'
  );
}
if (/^RNQNX_ENABLE_ACCESSIBILITY_BRIDGE:BOOL=ON$/m.test(cache)) {
  expectedTests.push(
    'rnqnx_accessibility_view_test',
    'rnqnx_accessibility_action_probe',
    'rnqnx_accessibility_action_native_test',
    'rnqnx_accessibility_action_rejects_interception',
    'rnqnx_accessibility_action_rejects_registration'
  );
}
const disabledV1Tests = [];
for (const [feature, tests] of [
  ['MODAL_FOCUS', ['rnqnx_ModalFocus_test']],
  ['DEV_RUNTIME', ['rnqnx_dev_session_host_test']],
  ['FRAME_TRACE', ['rnqnx_qt_frame_trace_test']],
]) {
  if (new RegExp(`^RNQNX_ENABLE_${feature}:BOOL=ON$`, 'm').test(cache)) {
    expectedTests.push(...tests);
  } else {
    disabledV1Tests.push(...tests);
  }
}
run('cmake', ['--build', '--preset', preset]);
const inventory = run('ctest', ['--test-dir', build, '--show-only=json-v1'], true);
const registered = new Set(
  JSON.parse(inventory).tests.map(/** @param {{name: string}} test */ (test) => test.name)
);
for (const name of expectedTests) {
  if (!registered.has(name)) throw new Error(`Required host-renderer test missing: ${name}`);
}
for (const name of disabledV1Tests) {
  if (registered.has(name)) throw new Error(`Disabled v1 feature registered a test: ${name}`);
}
mkdirSync(path.join(build, 'evidence'), { recursive: true });
writeFileSync(path.join(build, 'evidence', 'ctest-inventory.json'), inventory);
run('ctest', ['--preset', preset, '--output-junit', path.join(build, 'evidence', 'ctest.xml')]);
console.log(
  'RNQNX_FULL_HOST_RENDERER_OK Qt Hermes Fabric Metro host tests passed (not QNX target proof)'
);
