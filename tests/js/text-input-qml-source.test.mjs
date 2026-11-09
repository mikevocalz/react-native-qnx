import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import path from 'node:path';
import os from 'node:os';
const repo = process.cwd();
const temporary = mkdtempSync(path.join(os.tmpdir(), 'rnqnx-text-qml-'));
try {
  const source = path.join(temporary, 'main.cpp');
  writeFileSync(
    source,
    '#include "react-native-qnx/views/QtTextInputQml.h"\n#include <iostream>\nint main(){std::cout<<rnqnx::views::textInputQml();}\n'
  );
  for (const enabled of [0, 1]) {
    const binary = path.join(temporary, `qml-${enabled}`);
    const build = spawnSync(
      'c++',
      [
        '-std=c++20',
        `-DRNQNX_ENABLE_TEXT_INPUT_CONTRACT=${enabled}`,
        '-I',
        path.join(repo, 'vnext/include'),
        source,
        path.join(repo, 'vnext/src/views/qt/QtTextInputQml.cpp'),
        '-o',
        binary,
      ],
      { encoding: 'utf8', timeout: 30000 }
    );
    assert.equal(build.status, 0, build.stderr);
    const result = spawnSync(binary, [], { timeout: 5000 });
    assert.equal(result.status, 0);
    const qml = result.stdout.toString();
    // Both event-contract modes share the styled native field and Yoga content
    // insets. This compiled-source check does not establish rendered geometry.
    assert.match(qml, /objectName: "qnx-text-input-background"/);
    assert.match(qml, /color: root\.backgroundColor/);
    assert.match(qml, /border\.width: root\.borderWidth/);
    assert.match(qml, /border\.color: root\.borderColor/);
    assert.match(qml, /radius: root\.radius/);
    const textItems = [...qml.matchAll(/\n  (TextInput|TextEdit|Text) \{\n([\s\S]*?)\n  \}/g)];
    assert.deepEqual(
      textItems.map((item) => item[1]),
      ['TextInput', 'TextEdit', 'Text']
    );
    for (const [, , item] of textItems) {
      for (const edge of ['Left', 'Top', 'Right', 'Bottom']) {
        assert.ok(
          item.includes(`anchors.${edge.toLowerCase()}Margin: root.contentInset${edge}`),
          `${enabled ? 'ON' : 'OFF'} text item must use its ${edge} Yoga content inset`
        );
      }
      assert.match(item, /clip: true/);
      assert.match(item, /font: root\.font/);
    }
    if (enabled === 0) {
      // Reviewed styled-field snapshot; the legacy event mode remains free of
      // the opt-in selection endpoint observers.
      assert.equal(result.stdout.length, 4074);
      assert.equal(
        createHash('sha256').update(result.stdout).digest('hex'),
        'ebf863efcb8ae5a4e9eb0b60f9fb74252093d0e79d35b167771a4fa4b7000996'
      );
      assert.doesNotMatch(qml, /contractEnabled|onSelectionStartChanged|onSelectionEndChanged/);
    } else {
      assert.match(qml, /contractEnabled/);
      assert.equal((qml.match(/onSelectionStartChanged/g) || []).length, 2);
      assert.equal((qml.match(/onSelectionEndChanged/g) || []).length, 2);
    }
  }
  console.log(
    'Shared TextInput QML: styled OFF snapshot, both-mode content insets, and ON-only endpoint observers passed (compiled source; native layout separate).'
  );
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
