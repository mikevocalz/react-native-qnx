#!/usr/bin/env node
// Regenerates template/qnx/assets/icon.png, the default app icon that
// `rnqnx project create` copies into new projects: a 1024x1024 RGBA PNG with a
// full-bleed square background (no baked corner mask) and a mark that stays
// inside the central 66% safe zone. Deterministic: rerunning writes the same bytes.
import { writeFile } from 'node:fs/promises';
import path from 'node:path';
import { crc32, deflateSync } from 'node:zlib';

const size = 1024;
const background = [0x29, 0x6e, 0xd8];
const mark = [0xff, 0xff, 0xff];
const center = (size - 1) / 2;
// Ring and dot, both well inside the 0.66 * size safe-zone circle.
const ringOuter = size * 0.3;
const ringInner = size * 0.21;
const dot = size * 0.09;
const samples = 4;

/** @param {number} x @param {number} y */
function covered(x, y) {
  const distance = Math.hypot(x - center, y - center);
  return distance <= dot || (distance >= ringInner && distance <= ringOuter);
}

const rows = Buffer.alloc((size * 4 + 1) * size);
for (let y = 0; y < size; y++) {
  const row = y * (size * 4 + 1);
  rows[row] = 0; // filter: none
  for (let x = 0; x < size; x++) {
    let hits = 0;
    for (let sy = 0; sy < samples; sy++)
      for (let sx = 0; sx < samples; sx++)
        if (covered(x + (sx + 0.5) / samples - 0.5, y + (sy + 0.5) / samples - 0.5)) hits++;
    const t = hits / (samples * samples);
    const offset = row + 1 + x * 4;
    for (let c = 0; c < 3; c++)
      rows[offset + c] = Math.round(background[c] * (1 - t) + mark[c] * t);
    rows[offset + 3] = 0xff;
  }
}

/** @param {string} type @param {Buffer} data */
function chunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, 'latin1'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([length, body, crc]);
}

const header = Buffer.alloc(13);
header.writeUInt32BE(size, 0);
header.writeUInt32BE(size, 4);
header[8] = 8; // bit depth
header[9] = 6; // RGBA
const png = Buffer.concat([
  Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
  chunk('IHDR', header),
  chunk('IDAT', deflateSync(rows, { level: 9 })),
  chunk('IEND', Buffer.alloc(0)),
]);
const output = path.join(import.meta.dirname, '..', 'template', 'qnx', 'assets', 'icon.png');
await writeFile(output, png);
console.log(`${output} (${png.length} bytes)`);
