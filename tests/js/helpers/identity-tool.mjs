import { chmod, mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

// The JS lanes have no Qt. They exercise prepare-native and doctor wiring with
// a stand-in that speaks the rnqnx_identity_tool protocol: solid-color PNGs
// for `icon` and 10 px per code point for `label`. RNQNX_FAKE_HOST_VARIANT=1
// changes every pixel hash and width slightly, like another Qt or font engine. It proves nothing about Qt
// scaling or text measurement; native/app-identity's CTest lane covers those.
// Set RNQNX_IDENTITY_TOOL to run these tests against the real tool instead.
const fake = String.raw`#!/usr/bin/env node
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { crc32, deflateSync } from 'node:zlib';
const args = process.argv.slice(2);
const values = (name) => args.flatMap((arg, i) => (arg === '--' + name ? [args[i + 1]] : []));
function chunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, 'latin1'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([length, body, crc]);
}
function png(size) {
  const header = Buffer.alloc(13);
  header.writeUInt32BE(size, 0);
  header.writeUInt32BE(size, 4);
  header[8] = 8;
  header[9] = 6;
  const rows = Buffer.alloc((size * 4 + 1) * size, process.env.RNQNX_FAKE_HOST_VARIANT ? 0xfe : 0xff);
  for (let y = 0; y < size; y++) rows[y * (size * 4 + 1)] = 0;
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', header), chunk('IDAT', deflateSync(rows)), chunk('IEND', Buffer.alloc(0))]);
}
const width = (text) => [...text].length * 10 + (process.env.RNQNX_FAKE_HOST_VARIANT ? 0.25 : 0);
if (args[0] === 'icon') {
  const out = values('out-dir')[0];
  mkdirSync(out, { recursive: true });
  const sizes = values('size').map(Number).map((size) => {
    const file = path.join(out, 'icon-' + size + '.png');
    writeFileSync(file, png(size));
    return { size, path: file };
  });
  console.log(JSON.stringify({ sizes, scaler: 'test stand-in' }));
} else if (args[0] === 'label') {
  const text = Buffer.from(values('text-hex')[0], 'hex').toString('utf8');
  const box = Number(values('box-width')[0]);
  const limit = box - Number(values('margin')[0] ?? 0);
  const measured = width(text);
  let suggestion;
  const words = text.split(' ');
  for (let i = 1; i < words.length; i++) {
    const prefix = words.slice(0, i).join(' ');
    if (width(prefix) <= limit && !/[<&]/.test(prefix)) suggestion = prefix;
  }
  console.log(JSON.stringify({ widthPx: measured, boxWidthPx: box, overflowPx: Math.max(0, measured - limit),
    fits: measured <= limit, lineCount: 1, mightBeRichText: /<[a-z!/]/i.test(text), fallbackCodePoints: [], ...(suggestion ? { suggestion } : {}) }));
} else {
  process.exit(2);
}
`;

/** Always the stand-in, for cases that need to vary host output.
 * @returns {Promise<string>} */
export async function standInIdentityTool() {
  const directory = await mkdtemp(path.join(tmpdir(), 'rnqnx-identity-tool-'));
  const tool = path.join(directory, 'fake-identity-tool.mjs');
  await writeFile(tool, fake);
  await chmod(tool, 0o755);
  return tool;
}

/** @returns {Promise<{tool: string, real: boolean}>} */
export async function identityToolForTests() {
  if (process.env.RNQNX_IDENTITY_TOOL) return { tool: process.env.RNQNX_IDENTITY_TOOL, real: true };
  const directory = await mkdtemp(path.join(tmpdir(), 'rnqnx-identity-tool-'));
  const tool = path.join(directory, 'fake-identity-tool.mjs');
  await writeFile(tool, fake);
  await chmod(tool, 0o755);
  return { tool, real: false };
}
