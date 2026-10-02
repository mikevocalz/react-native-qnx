#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import path from 'node:path';

const root = process.cwd();
const read = (relative) => readFile(path.join(root, relative), 'utf8');

function requireText(source, text, label) {
  if (!source.includes(text)) {
    throw new Error(`${label}: missing ${JSON.stringify(text)}`);
  }
}

const helper = await read('scripts/lib/qnx-sdp.sh');
const bootstrap = await read('scripts/bootstrap-qt-qnx.sh');
const installPi = await read('scripts/install-pi.sh');
const runtime = await read('scripts/validate-runtime-qnx.sh');

requireText(helper, 'rnqnx_source_sdp_environment()', 'shared QNX SDP helper');
requireText(helper, 'QNX_SDP_ENV', 'shared QNX SDP helper');

for (const [label, source] of [
  ['Pi installer', installPi],
  ['runtime validator', runtime],
  ['Qt bootstrap', bootstrap],
]) {
  requireText(source, 'scripts/lib/qnx-sdp.sh', label);
  requireText(source, 'rnqnx_source_sdp_environment', label);
}

for (const token of [
  'RNQNX_QT_SOURCE_DIR',
  'RNQNX_QT_HOST_PATH',
  '-submodules qtbase,qtdeclarative',
  '-qt-host-path',
  '-extprefix',
  '-DCMAKE_TOOLCHAIN_FILE=',
  'lib/cmake/Qt6/Qt6Config.cmake',
  'lib/cmake/Qt6/qt.toolchain.cmake',
  'libQt6${module}.so',
]) {
  requireText(bootstrap, token, 'Qt QNX bootstrap');
}

console.log('validate:qnx-qt-bootstrap passed');
