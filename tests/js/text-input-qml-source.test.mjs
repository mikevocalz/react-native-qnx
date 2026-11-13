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
    if (enabled === 0) {
      // Baseline updated 2026-11-13: clipping, view background and padding.
      assert.equal(result.stdout.length, 3784);
      assert.equal(
        createHash('sha256').update(result.stdout).digest('hex'),
        'abb15de203c795e5f425997ed1e4942b14ffb6ca2d6593fdaa92907ed10ea206'
      );
    } else {
      assert.match(result.stdout.toString(), /contractEnabled/);
      assert.match(result.stdout.toString(), /onSelectionStartChanged/);
    }
  }
  console.log(
    'Shared TextInput QML: compiled OFF bytes match baseline; ON endpoint observers present.'
  );
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
