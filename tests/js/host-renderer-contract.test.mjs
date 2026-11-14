import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

// Exercise orchestration with fake executables, not pretend renderer proof.
// Actual Qt/Hermes/Fabric tests run in the dedicated native workflow.
if (process.platform === 'linux') {
  const root = mkdtempSync(path.join(os.tmpdir(), 'rnqnx-host-renderer-contract-'));
  const script = fileURLToPath(
    new URL('../../scripts/validate-host-renderer.mjs', import.meta.url)
  );
  const names = [
    'rnqnx_g0_cxx20_probe',
    'qnx_screen_surface_host_test',
    'rnqnx_hermes_probe',
    'qt_react_host_host_test',
    'rnqnx_rn_headers_probe',
    'shadow_view_mutation_adapter_host_test',
    'shadow_tree_mounting_host_test',
    'qtquick_mounting_layer_host_test',
    'rnqnx_qtquick_host_probe',
  ];
  try {
    const bin = path.join(root, 'bin');
    mkdirSync(bin);
    writeFileSync(path.join(bin, 'cmake'), '#!/bin/sh\nexit "${FAKE_BUILD_STATUS:-0}"\n', {
      mode: 0o755,
    });
    writeFileSync(
      path.join(bin, 'ctest'),
      `#!${process.execPath}\nif (process.argv.includes('--show-only=json-v1')) console.log(process.env.FAKE_INVENTORY); else process.exit(Number(process.env.FAKE_TEST_STATUS || 0));\n`,
      { mode: 0o755 }
    );
    /** @param {Record<string, string>} overrides */
    function invoke(overrides = {}) {
      return spawnSync(process.execPath, [script], {
        cwd: root,
        encoding: 'utf8',
        env: {
          ...process.env,
          PATH: `${bin}:${process.env.PATH}`,
          FAKE_INVENTORY: JSON.stringify({ tests: names.map((name) => ({ name })) }),
          ...overrides,
        },
      });
    }
    const build = path.join(root, 'build/linux-host-renderer');
    mkdirSync(build, { recursive: true });
    const cache = path.join(build, 'CMakeCache.txt');
    const bothOff =
      'RNQNX_ENABLE_TEXT_INPUT_CONTRACT:BOOL=OFF\nRNQNX_ENABLE_ACCESSIBILITY_BRIDGE:BOOL=OFF\n';
    writeFileSync(cache, bothOff);
    assert.equal(invoke().status, 0);
    assert.equal(
      JSON.parse(
        readFileSync(
          path.join(root, 'build/linux-host-renderer/evidence/ctest-inventory.json'),
          'utf8'
        )
      ).tests.length,
      names.length
    );
    for (const missing of names) {
      const result = invoke({
        FAKE_INVENTORY: JSON.stringify({
          tests: names.filter((name) => name !== missing).map((name) => ({ name })),
        }),
      });
      assert.notEqual(result.status, 0);
      assert.match(result.stderr, new RegExp(`Required host-renderer test missing: ${missing}`));
    }
    const textTests = [
      'rnqnx_text_input_probe',
      'rnqnx_text_input_bridge_test',
      'rnqnx_text_input_rejects_interception',
    ];
    writeFileSync(cache, 'RNQNX_ENABLE_TEXT_INPUT_CONTRACT:BOOL=ON\n');
    const allNames = [...names, ...textTests];
    assert.equal(
      invoke({ FAKE_INVENTORY: JSON.stringify({ tests: allNames.map((name) => ({ name })) }) })
        .status,
      0
    );
    for (const missing of textTests) {
      const result = invoke({
        FAKE_INVENTORY: JSON.stringify({
          tests: allNames.filter((name) => name !== missing).map((name) => ({ name })),
        }),
      });
      assert.notEqual(result.status, 0);
      assert.match(result.stderr, new RegExp(`Required host-renderer test missing: ${missing}`));
    }
    writeFileSync(
      cache,
      'RNQNX_ENABLE_TEXT_INPUT_CONTRACT:BOOL=OFF\nRNQNX_ENABLE_ACCESSIBILITY_BRIDGE:BOOL=ON\n'
    );
    const accessibilityTests = [
      'rnqnx_accessibility_view_test',
      'rnqnx_accessibility_action_probe',
      'rnqnx_accessibility_action_native_test',
      'rnqnx_accessibility_action_rejects_interception',
      'rnqnx_accessibility_action_rejects_registration',
    ];
    const accessibilityPresent = invoke({
      FAKE_INVENTORY: JSON.stringify({
        tests: [...names, ...accessibilityTests].map((name) => ({ name })),
      }),
    });
    assert.equal(accessibilityPresent.status, 0, accessibilityPresent.stderr);
    for (const missing of accessibilityTests) {
      const result = invoke({
        FAKE_INVENTORY: JSON.stringify({
          tests: [...names, ...accessibilityTests]
            .filter((name) => name !== missing)
            .map((name) => ({ name })),
        }),
      });
      assert.notEqual(result.status, 0);
      assert.match(result.stderr, new RegExp(`Required host-renderer test missing: ${missing}`));
    }
    writeFileSync(
      cache,
      'RNQNX_ENABLE_TEXT_INPUT_CONTRACT:BOOL=ON\nRNQNX_ENABLE_ACCESSIBILITY_BRIDGE:BOOL=ON\n'
    );
    const bothPresent = invoke({
      FAKE_INVENTORY: JSON.stringify({
        tests: [...allNames, ...accessibilityTests].map((name) => ({ name })),
      }),
    });
    assert.equal(bothPresent.status, 0, bothPresent.stderr);
    // Feature-enabled CI must run the actual native and React/Fabric SVG
    // tests. A successful baseline-only inventory is not v2 runtime proof.
    const qtFeatures = [
      ['SVG', ['rnqnx_qt_svg_test', 'rnqnx_qt_svg_fabric_test']],
      [
        'RNSVG',
        [
          'rnqnx_qt_rnsvg_test',
          'rnqnx_qt_rnsvg_scene_test',
          'rnqnx_qt_rnsvg_filter_test',
          'rnqnx_qt_rnsvg_text_test',
          'rnqnx_qt_rnsvg_image_test',
          'rnqnx_qt_rnsvg_foreignobject_test',
          'rnqnx_qt_rnsvg_loader_test',
          'rnqnx_qt_rnsvg_fixtures_test',
          'rnqnx_qt_rnsvg_fabric_test',
          'rnqnx_qt_rnsvg_image_fabric_test',
          'rnqnx_qt_rnsvg_corpus_test',
          'rnqnx_qt_rnsvg_corpus_test_1',
          'rnqnx_qt_rnsvg_corpus_test_2',
          'rnqnx_qt_rnsvg_corpus_test_3',
          'rnqnx_qt_rnsvg_values_test',
        ],
      ],
      ['MOTION', ['rnqnx_qt_motion_test']],
      ['GRAPHICS', ['rnqnx_qt_graphics', 'rnqnx_qt_graphics_null']],
    ];
    for (const [feature, tests] of qtFeatures) {
      writeFileSync(cache, bothOff + `RNQNX_ENABLE_QT_${feature}:BOOL=ON\n`);
      const present = [...names, ...tests];
      const accepted = invoke({
        FAKE_INVENTORY: JSON.stringify({ tests: present.map((name) => ({ name })) }),
      });
      assert.equal(accepted.status, 0, accepted.stderr);
      for (const missing of tests) {
        const result = invoke({
          FAKE_INVENTORY: JSON.stringify({
            tests: present.filter((name) => name !== missing).map((name) => ({ name })),
          }),
        });
        assert.notEqual(result.status, 0);
        assert.match(result.stderr, new RegExp(`Required host-renderer test missing: ${missing}`));
      }
      writeFileSync(cache, bothOff + `RNQNX_ENABLE_QT_${feature}:BOOL=OFF\n`);
      const unexpected = invoke({
        FAKE_INVENTORY: JSON.stringify({ tests: present.map((name) => ({ name })) }),
      });
      assert.notEqual(unexpected.status, 0);
      assert.match(unexpected.stderr, /Disabled Qt feature registered a test:/);
    }
    writeFileSync(cache, bothOff);
    assert.notEqual(invoke({ FAKE_BUILD_STATUS: '7' }).status, 0);
    const testFailure = invoke({ FAKE_TEST_STATUS: '8' });
    assert.notEqual(testFailure.status, 0);
    assert.match(testFailure.stderr, /ctest failed \(8\)/);
    assert.notEqual(invoke({ FAKE_INVENTORY: '{invalid' }).status, 0);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
}
